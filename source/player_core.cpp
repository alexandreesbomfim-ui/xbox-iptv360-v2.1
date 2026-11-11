#include "player.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

namespace {

// Fila circular de audio (int16 intercalado, estereo)
const int kAudioQueueSamples = 48000 * 2 * 2;   // 2 s

struct Ctx {
    PlayerPlatform* plat;
    plm_frame_t*    last_frame;
    int             frames_decoded;
    int16_t*        aq;
    int             aq_head, aq_count;       // em amostras int16
    int             underruns;
    bool            audio_started;
    bool            in_underrun;
};

void on_video(plm_t*, plm_frame_t* frame, void* user) {
    Ctx* c = (Ctx*)user;
    c->last_frame = frame;
    c->frames_decoded++;
}

void on_audio(plm_t*, plm_samples_t* s, void* user) {
    Ctx* c = (Ctx*)user;
    int n = (int)s->count * 2;
    for (int i = 0; i < n; ++i) {
        if (c->aq_count >= kAudioQueueSamples) return;   // fila cheia: descarta
        float f = s->interleaved[i];
        if (f > 1.0f) f = 1.0f; else if (f < -1.0f) f = -1.0f;
        int16_t v = (int16_t)(f * 32767.0f);
        c->aq[(c->aq_head + c->aq_count) % kAudioQueueSamples] = v;
        c->aq_count++;
    }
}

// Envia ao hardware o que couber. Serializa em little-endian explicitamente
// (o PowerPC do Xbox 360 e big-endian; o driver espera s16 LE).
void pump_audio(Ctx* c) {
    int freeBytes = c->plat->audio_free_bytes();
    int can = freeBytes / 4;                       // quadros estereo (4 bytes)
    int have = c->aq_count / 2;
    int frames = can < have ? can : have;
    if (frames <= 0) return;
    static uint8_t tmp[4096 * 4];
    while (frames > 0) {
        int chunk = frames > 4096 ? 4096 : frames;
        for (int i = 0; i < chunk * 2; ++i) {
            int16_t v = c->aq[c->aq_head];
            c->aq_head = (c->aq_head + 1) % kAudioQueueSamples;
            tmp[i * 2]     = (uint8_t)(v & 0xFF);
            tmp[i * 2 + 1] = (uint8_t)((uint16_t)v >> 8);
        }
        c->aq_count -= chunk * 2;
        c->plat->audio_submit(tmp, chunk * 4);
        frames -= chunk;
    }
}

}  // namespace

bool player_run(const char* path, PlayerPlatform& plat, PlayerStats& st, double max_seconds) {
    memset(&st, 0, sizeof(st));
    plm_t* plm = plm_create_with_filename(path);
    if (!plm) return false;

    // Precisa ler o cabecalho para saber dimensoes
    if (!plm_probe(plm, 5000 * 1024)) {
        plm_destroy(plm);
        return false;
    }
    int w = plm_get_width(plm), h = plm_get_height(plm);
    if (w <= 0 || h <= 0) { plm_destroy(plm); return false; }
    st.width = w; st.height = h;
    st.samplerate = plm_get_num_audio_streams(plm) > 0 ? plm_get_samplerate(plm) : 0;

    Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.plat = &plat;
    ctx.aq = (int16_t*)malloc(sizeof(int16_t) * kAudioQueueSamples);
    uint32_t* bgra = (uint32_t*)calloc((size_t)w * h, 4);   // calloc: byte "x" = 0
    if (!ctx.aq || !bgra) { free(ctx.aq); free(bgra); plm_destroy(plm); return false; }

    plm_set_video_decode_callback(plm, on_video, &ctx);
    plm_set_audio_decode_callback(plm, on_audio, &ctx);
    plm_set_loop(plm, 0);
    plm_set_audio_enabled(plm, st.samplerate != 0);
    plm_set_audio_lead_time(plm, 0.2);

    uint64_t start = plat.now_ms(), last = start, lastStat = start;
    int framesAtStat = 0;
    bool quit = false;

    while (!plm_has_ended(plm) && !quit) {
        uint64_t now = plat.now_ms();
        double dt = (double)(now - last) / 1000.0;
        last = now;
        if (dt > 0.25) dt = 0.25;                  // evita saltos apos travadas

        ctx.last_frame = 0;
        int before = ctx.frames_decoded;
        plm_decode(plm, dt);

        if (ctx.last_frame) {
            plm_frame_to_bgra(ctx.last_frame, (uint8_t*)bgra, w * 4);
            plat.present(bgra, w, h);
            st.frames_shown++;
        } else if (ctx.frames_decoded == before) {
            plat.idle();
        }

        if (st.samplerate) {
            int before_q = ctx.aq_count;
            pump_audio(&ctx);
            if (ctx.aq_count < before_q) ctx.audio_started = true;   // ja enviou algo ao hardware
            // Falta de audio = hardware sem nada para tocar (conta so a transicao)
            if (ctx.audio_started && !plm_has_ended(plm)) {
                if (plat.audio_unplayed_bytes() == 0) {
                    if (!ctx.in_underrun) { ctx.underruns++; ctx.in_underrun = true; }
                } else {
                    ctx.in_underrun = false;
                }
            }
        }

        now = plat.now_ms();
        double real = (double)(now - start) / 1000.0;
        if (now - lastStat >= 1000) {
            double sec = (double)(now - lastStat) / 1000.0;
            char line[160];
            snprintf(line, sizeof(line), "%dx%d  %.1f fps  video %.1fs / real %.1fs  x%.2f  buf_audio=%d",
                     w, h, (ctx.frames_decoded - framesAtStat) / sec,
                     plm_get_time(plm), real, real > 0 ? plm_get_time(plm) / real : 0.0,
                     ctx.aq_count / 2);
            plat.status(line);
            lastStat = now;
            framesAtStat = ctx.frames_decoded;
        }
        if (max_seconds > 0 && real >= max_seconds) break;
        if (plat.quit_requested()) quit = true;
    }

    uint64_t end = plat.now_ms();
    st.real_seconds = (double)(end - start) / 1000.0;
    st.video_seconds = plm_get_time(plm);
    st.frames_decoded = ctx.frames_decoded;
    st.decode_fps = st.real_seconds > 0 ? ctx.frames_decoded / st.real_seconds : 0;
    st.speed = st.real_seconds > 0 ? st.video_seconds / st.real_seconds : 0;
    st.audio_underruns = ctx.underruns;

    free(ctx.aq);
    free(bgra);
    plm_destroy(plm);
    return true;
}
