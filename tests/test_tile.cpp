// Garante que tile_index() == formula original do console_pset() do libxenon
#include "../source/tile.h"
#include <assert.h>
#include <stdio.h>

int main() {
    for (int W = 1280; W <= 1920; W += 640) {
        int W32 = ((W + 31) >> 5) << 5;
        for (int y = 0; y < 1088; ++y)
            for (int x = 0; x < W; ++x) {
                uint32_t ref = (((y >> 5) * 32 * W32 + ((x >> 5) << 10)
                    + (x & 3) + ((y & 1) << 2) + (((x & 31) >> 2) << 3) + (((y & 31) >> 1) << 6)) ^ ((y & 8) << 2));
                assert(tile_index(x, y, W32) == ref);
            }
    }
    puts("tile OK");
    return 0;
}
