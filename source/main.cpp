// IPTV360 - gerenciador de listas M3U para Xbox 360 (libxenon / Free60)
// Roda via XeLL (.elf32). NAO e um XEX.
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

extern "C" {
#include <xenos/xenos.h>
#include <console/console.h>
#include <input/input.h>
#include <usb/usbmain.h>
#include <diskio/ata.h>
#include <time/time.h>
#include <ppc/timebase.h>
#include <ppc/cache.h>
#include <xenon_soc/xenon_power.h>
#include <libfat/fat.h>
int bdev_enum(int handle, const char** name);  // definido no newlib do libxenon
extern char bss_start[];
}

#include "m3u.h"
#include "player_xenon.h"

static const int   STICK_THRESHOLD = 20000;
static const int   PAGE_ROWS       = 24;
static const char* kSearchPaths[]  = { "playlist.m3u", "iptv360/playlist.m3u", "playlist.m3u8", 0 };

static std::vector<Channel> g_all;      // todos os canais
static std::vector<int>     g_view;     // indices visiveis (apos filtro)
static std::vector<std::string> g_groups;
static int g_groupSel = -1;             // -1 = todos
static int g_sel = 0;
static std::string g_source;

static void poll_pad(controller_data_s& pad) {
    usb_do_poll();
    get_controller_data(&pad, 0);
}

static void rebuild_groups() {
    g_groups.clear();
    for (size_t i = 0; i < g_all.size(); ++i) {
        const std::string& g = g_all[i].group;
        if (g.empty()) continue;
        bool found = false;
        for (size_t k = 0; k < g_groups.size(); ++k)
            if (g_groups[k] == g) { found = true; break; }
        if (!found) g_groups.push_back(g);
    }
}

static void rebuild_view() {
    g_view.clear();
    for (size_t i = 0; i < g_all.size(); ++i)
        if (g_groupSel < 0 || g_all[i].group == g_groups[g_groupSel])
            g_view.push_back((int)i);
    g_sel = 0;
}

static bool load_playlist() {
    g_all.clear();
    g_source.clear();
    int h = -1;
    const char* dev;
    while ((h = bdev_enum(h, &dev)) >= 0) {
        for (int i = 0; kSearchPaths[i]; ++i) {
            std::string p = std::string(dev) + ":/" + kSearchPaths[i];
            if (m3u_load(p.c_str(), g_all)) {
                g_source = p;
                if (!g_all.empty()) goto done;
            }
        }
    }
done:
    rebuild_groups();
    g_groupSel = -1;
    rebuild_view();
    return !g_all.empty();
}

static std::string clip(const std::string& s, size_t n) {
    return s.size() <= n ? s : s.substr(0, n - 3) + "...";
}

static void draw_list() {
    console_set_colors(0, -1);
    console_clrscr();
    printf("IPTV360 | %s | %d canais%s%s\n",
           g_source.c_str(), (int)g_view.size(),
           g_groupSel >= 0 ? " | grupo: " : "",
           g_groupSel >= 0 ? g_groups[g_groupSel].c_str() : "");
    printf("A: detalhes  Y: grupo  B: recarregar  LB/RB: pagina  Logo: sair\n");
    printf("X: teste video 352x288 (test_352.mpg)   Start: teste 640x480 (test_640.mpg)\n\n");

    int start = g_sel - PAGE_ROWS / 2;
    if (start < 0) start = 0;
    int end = start + PAGE_ROWS;
    if (end > (int)g_view.size()) end = (int)g_view.size();

    for (int i = start; i < end; ++i) {
        const Channel& c = g_all[g_view[i]];
        if (i == g_sel) console_set_colors(-1, 0); else console_set_colors(0, -1);
        printf("%5d  %s\n", i + 1, clip(c.name, 70).c_str());
    }
    console_set_colors(0, -1);
}

static void draw_details() {
    const Channel& c = g_all[g_view[g_sel]];
    console_set_colors(0, -1);
    console_clrscr();
    printf("Canal : %s\nGrupo : %s\n\nURL   :\n%s\n\n", c.name.c_str(),
           c.group.empty() ? "-" : c.group.c_str(), c.url.c_str());
    printf("Reproducao de video ainda nao implementada.\n");
    printf("(libxenon nao inclui decodificador de video/audio de streams)\n\n");
    printf("B: voltar\n");
}

// Procura um arquivo em todos os dispositivos (raiz e pasta iptv360/)
static bool find_file(const char* name, std::string& out) {
    static const char* dirs[] = { "", "iptv360/", 0 };
    int h = -1;
    const char* dev;
    while ((h = bdev_enum(h, &dev)) >= 0) {
        for (int i = 0; dirs[i]; ++i) {
            std::string p = std::string(dev) + ":/" + dirs[i] + name;
            FILE* f = fopen(p.c_str(), "rb");
            if (f) { fclose(f); out = p; return true; }
        }
    }
    return false;
}

static void wait_release_then_b() {
    controller_data_s pad;
    for (;;) {                       // espera soltar tudo
        poll_pad(pad);
        if (!(pad.a || pad.b || pad.x || pad.y || pad.start || pad.back || pad.logo)) break;
        mdelay(20);
    }
    for (;;) {                       // espera B
        poll_pad(pad);
        if (pad.b || pad.back || pad.logo) break;
        mdelay(20);
    }
}

// Teste de video: toca um .mpg e mostra o resultado de desempenho
static void run_video_test(const char* name) {
    std::string path;
    console_set_colors(0, -1);
    console_clrscr();
    if (!find_file(name, path)) {
        printf("Arquivo '%s' nao encontrado.\n", name);
        printf("Copie para a raiz do pendrive (ou pasta iptv360/).\n\nB: voltar\n");
        wait_release_then_b();
        return;
    }
    printf("Tocando %s ...\n", path.c_str());
    PlayerStats st;
    bool ok = player_play_xenon(path.c_str(), st);
    console_set_colors(0, -1);
    console_clrscr();
    if (!ok) {
        printf("Nao foi possivel abrir/decodificar '%s'.\n", path.c_str());
        printf("O arquivo deve ser MPEG-1 + MP2 (.mpg).\n\nB: voltar\n");
        wait_release_then_b();
        return;
    }
    printf("=== RESULTADO DO TESTE DE VIDEO ===\n\n");
    printf("Arquivo        : %s\n", path.c_str());
    printf("Resolucao      : %dx%d\n", st.width, st.height);
    printf("Audio          : %d Hz\n", st.samplerate);
    printf("Tempo real     : %.2f s\n", st.real_seconds);
    printf("Video tocado   : %.2f s\n", st.video_seconds);
    printf("Quadros        : %d decodificados, %d exibidos\n", st.frames_decoded, st.frames_shown);
    printf("FPS medio      : %.1f\n", st.decode_fps);
    printf("Velocidade     : x%.2f  (1.00 = tempo real)\n", st.speed);
    printf("Falhas de audio: %d\n\n", st.audio_underruns);
    if (st.speed >= 0.97 && st.audio_underruns == 0)
        printf("VEREDITO: ACOMPANHA O TEMPO REAL nesta resolucao.\n");
    else
        printf("VEREDITO: NAO acompanha o tempo real (video atrasa ou audio falha).\n");
    printf("\nB: voltar\n");
    wait_release_then_b();
}

int main() {
    memicbi((void*)0x80000000, (int)((unsigned long)bss_start - 0x80000000UL));
    xenos_init(VIDEO_MODE_AUTO);
    console_init();
    xenon_make_it_faster(XENON_SPEED_FULL);

    usb_init();
    usb_do_poll();
    xenon_ata_init();
    xenon_atapi_init();
    fatInitDefault();

    bool haveList = load_playlist();
    bool detail = false, dirty = true;
    controller_data_s pad, prev;
    memset(&pad, 0, sizeof(pad));
    memset(&prev, 0, sizeof(prev));
    uint64_t lastMove = 0;

    for (;;) {
        poll_pad(pad);
        uint64_t now = mftb();
        bool canMove = tb_diff_msec(now, lastMove) > 120;

        if (pad.logo) return 0;  // volta ao XeLL

        if (!detail && ((pad.x && !prev.x) || (pad.start && !prev.start))) {
            run_video_test(pad.x ? "test_352.mpg" : "test_640.mpg");
            memset(&pad, 0, sizeof(pad));
            memset(&prev, 0, sizeof(prev));
            dirty = true;
            continue;
        }

        if (detail) {
            if (pad.b && !prev.b) { detail = false; dirty = true; }
        } else {
            int n = (int)g_view.size();
            int up   = (pad.up   || pad.s1_y >  STICK_THRESHOLD);
            int down = (pad.down || pad.s1_y < -STICK_THRESHOLD);
            if (n && canMove && up)   { g_sel = (g_sel + n - 1) % n; lastMove = now; dirty = true; }
            if (n && canMove && down) { g_sel = (g_sel + 1) % n;     lastMove = now; dirty = true; }
            if (n && pad.lb && !prev.lb) { g_sel -= PAGE_ROWS; if (g_sel < 0) g_sel = 0; dirty = true; }
            if (n && pad.rb && !prev.rb) { g_sel += PAGE_ROWS; if (g_sel >= n) g_sel = n - 1; dirty = true; }
            if (n && pad.a && !prev.a)   { detail = true; dirty = true; }
            if (pad.y && !prev.y && !g_groups.empty()) {
                g_groupSel++;
                if (g_groupSel >= (int)g_groups.size()) g_groupSel = -1;
                rebuild_view();
                dirty = true;
            }
            if (pad.b && !prev.b) { haveList = load_playlist(); dirty = true; }
        }
        prev = pad;

        if (dirty) {
            dirty = false;
            if (!haveList) {
                console_set_colors(0, -1);
                console_clrscr();
                printf("IPTV360\n\nNenhuma playlist encontrada.\n");
                printf("Coloque 'playlist.m3u' na raiz do pendrive USB (FAT32)\n");
                printf("ou em 'iptv360/playlist.m3u'.\n\nB: tentar de novo   Logo: sair\n");
                printf("X: teste video 352x288   Start: teste video 640x480\n");
            } else if (detail) {
                draw_details();
            } else {
                draw_list();
            }
        }
        mdelay(16);
    }
}
