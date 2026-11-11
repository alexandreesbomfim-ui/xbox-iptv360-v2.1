// Plataforma Xbox 360 (libxenon) para o player: framebuffer, audio e controle.
#include "player_xenon.h"
#include "tile.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include <xenos/xenos.h>
#include <console/console.h>
#include <input/input.h>
#include <usb/usbmain.h>
#include <time/time.h>
#include <ppc/timebase.h>
#include <ppc/cache.h>
#include <xenon_sound/sound.h>
}

namespace {

// Mesmo layout/endereco que o console do libxenon usa (console.c)
struct ati_info {
    uint32_t unknown1[4];
    uint32_t base;
    uint32_t unknown2[8];
    uint32_t width;
    uint32_t height;
} __attribute__((__packed__));

const int kMaxW = 2048;

class XenonPlatform : public PlayerPlatform {
public:
    XenonPlatform() : sound_ready(false), lastPoll(0), quit(false), curSW(0), curSH(0) {
        ati_info* ai = (ati_info*)0xec806100ULL;
        fb  = (uint32_t*)(long)(ai->base | 0x80000000);
        scrW = (int)ai->width;
        scrH = (int)ai->height;
        W32  = ((scrW + 31) >> 5) << 5;
        ox = oy = 0;
        if (xenos_is_overscan()) { ox = scrW / 28; oy = scrH / 28; }
        areaW = scrW - 2 * ox;
        areaH = scrH - 2 * oy;
        if (areaW > kMaxW) areaW = kMaxW;
        memset(&pad, 0, sizeof(pad));
        xenon_sound_init();
        sound_ready = true;
    }

    uint64_t now_ms() { return mftb() / (PPC_TIMEBASE_FREQ / 1000); }

    void present(const uint32_t* src, int sw, int sh) {
        if (sw != curSW || sh != curSH) rebuild(sw, sh);
        for (int dy = 0; dy < dh; ++dy) {
            const uint32_t* row = src + (size_t)(dy * sh / dh) * sw;
            int y = dy0 + dy;
            uint32_t yhi = tile_y_hi(y, W32), ylo = tile_y_lo(y);
            uint32_t* base = fb + yhi;
            for (int dx = 0; dx < dw; ++dx)
                base[xhi[dx] + (xlo[dx] ^ ylo)] = row[xs[dx]];
        }
        // Envia ao video apenas as faixas (de 32 linhas) alteradas
        int b0 = dy0 >> 5, b1 = (dy0 + dh - 1) >> 5;
        memdcbst(fb + (size_t)b0 * 32 * W32, (b1 - b0 + 1) * 32 * W32 * 4);
    }

    int  audio_free_bytes()     { return xenon_sound_get_free(); }
    int  audio_unplayed_bytes() { return xenon_sound_get_unplayed(); }
    void audio_submit(const void* d, int n) { xenon_sound_submit((void*)d, n); }
    void idle() { mdelay(1); }

    bool quit_requested() {
        uint64_t t = now_ms();
        if (t - lastPoll >= 50) {          // le o controle 20x por segundo
            lastPoll = t;
            usb_do_poll();
            get_controller_data(&pad, 0);
            if (pad.b || pad.back || pad.logo) quit = true;
        }
        return quit;
    }

    void status(const char* text) {
        char line[110];
        snprintf(line, sizeof(line), "%-100s", text);
        console_set_colors(0, -1);
        console_set_cursor(0, 0);
        printf("%s", line);
    }

private:
    void rebuild(int sw, int sh) {
        curSW = sw; curSH = sh;
        // Cabe na area segura mantendo proporcao (vizinho mais proximo)
        dw = areaW;
        dh = sh * dw / sw;
        if (dh > areaH) { dh = areaH; dw = sw * dh / sh; }
        if (dw > kMaxW) dw = kMaxW;
        dx0 = ox + (areaW - dw) / 2;
        dy0 = oy + (areaH - dh) / 2;
        for (int x = 0; x < dw; ++x) {
            xs[x]  = x * sw / dw;
            xhi[x] = tile_x_hi(dx0 + x);
            xlo[x] = tile_x_lo(dx0 + x);
        }
        console_set_colors(0, -1);
        console_clrscr();                  // barras pretas
    }

    uint32_t* fb;
    int scrW, scrH, W32, ox, oy, areaW, areaH;
    int dw, dh, dx0, dy0;
    bool sound_ready;
    uint64_t lastPoll;
    bool quit;
    int curSW, curSH;
    controller_data_s pad;
    int      xs[kMaxW];
    uint32_t xhi[kMaxW], xlo[kMaxW];
};

}  // namespace

bool player_play_xenon(const char* path, PlayerStats& st) {
    XenonPlatform* plat = new XenonPlatform();
    bool ok = player_run(path, *plat, st, 0.0);
    delete plat;
    return ok;
}
