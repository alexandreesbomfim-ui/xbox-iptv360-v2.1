// Teste no PC: usa o mesmo player_core do Xbox com uma plataforma falsa.
#include "../source/player.h"
#include "../source/tile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct HostPlat : PlayerPlatform {
    uint64_t t0;
    long audio_bytes;
    int presents;
    uint32_t* first;
    int fw, fh;
    const char* dump;
    HostPlat(const char* d) : audio_bytes(0), presents(0), first(0), fw(0), fh(0), dump(d) {
        t0 = now_ms();
    }
    uint64_t now_ms() { timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }
    void present(const uint32_t* b, int w, int h) {
        if (!first) { first = (uint32_t*)malloc((size_t)w * h * 4); memcpy(first, b, (size_t)w * h * 4); fw = w; fh = h; }
        presents++;
    }
    int  audio_free_bytes() { return 1 << 20; }
    // hardware simulado: toca 48000*4 bytes/s a partir do 1o envio
    uint64_t first_submit_ms = 0;
    int  audio_unplayed_bytes() {
        if (!first_submit_ms) return 0;
        double played = (now_ms() - first_submit_ms) / 1000.0 * 192000.0;
        double left = audio_bytes - played;
        return left > 0 ? (int)left : 0;
    }
    void audio_submit(const void* d, int n) { (void)d; if (!first_submit_ms) first_submit_ms = now_ms(); audio_bytes += n; }
    void idle() { timespec ts = {0, 1000000}; nanosleep(&ts, 0); }
    bool quit_requested() { return false; }
    void status(const char* s) { printf("  [status] %s\n", s); }
};

int main(int argc, char** argv) {
    assert(argc >= 3);
    const char* file = argv[1];
    const char* dump = argv[2];
    double secs = argc > 3 ? atof(argv[3]) : 3.0;

    HostPlat plat(dump);
    PlayerStats st;
    bool ok = player_run(file, plat, st, secs);
    assert(ok);
    printf("%dx%d audio=%d Hz | decoded=%d shown=%d | real=%.2fs video=%.2fs | %.1f fps x%.2f | audio=%ld bytes underruns=%d\n",
           st.width, st.height, st.samplerate, st.frames_decoded, st.frames_shown,
           st.real_seconds, st.video_seconds, st.decode_fps, st.speed, plat.audio_bytes, st.audio_underruns);

    FILE* f = fopen(dump, "wb");
    fwrite(plat.first, 4, (size_t)plat.fw * plat.fh, f);
    fclose(f);

    assert(st.audio_underruns <= 1);   // no maximo o inicio
    // O decodificador deve acompanhar o tempo real (no PC com folga)
    assert(st.speed > 0.9 && st.speed < 1.2);
    // audio ~ 48000 Hz * 4 bytes * tempo de video (tolerancia 0.5 s)
    double expect = 48000.0 * 4 * st.video_seconds;
    assert(st.samplerate == 48000);
    assert(plat.audio_bytes > expect - 48000.0 * 4 * 0.5);
    assert(plat.audio_bytes < expect + 48000.0 * 4 * 0.5);
    free(plat.first);
    puts("OK");
    return 0;
}
