// Endereco de pixel no framebuffer "tiled" do Xenos (blocos 32x32).
// Mesma formula usada por console_pset() no libxenon, separada em partes
// por coluna (x) e por linha (y) para permitir pre-calculo rapido.
#ifndef IPTV360_TILE_H
#define IPTV360_TILE_H

#include <stdint.h>

// W32 = largura do framebuffer arredondada para multiplo de 32
static inline uint32_t tile_x_hi(int x)            { return (uint32_t)(x >> 5) << 10; }
static inline uint32_t tile_x_lo(int x)            { return (uint32_t)((x & 3) + (((x & 31) >> 2) << 3)); }
static inline uint32_t tile_y_hi(int y, int W32)   { return (uint32_t)(y >> 5) * 32u * (uint32_t)W32; }
static inline uint32_t tile_y_lo(int y)            { return (uint32_t)((((y & 1) << 2) + (((y & 31) >> 1) << 6)) ^ ((y & 8) << 2)); }

// Indice (em pixels de 32 bits) do pixel (x,y)
static inline uint32_t tile_index(int x, int y, int W32) {
    return tile_x_hi(x) + tile_y_hi(y, W32) + (tile_x_lo(x) ^ tile_y_lo(y));
}

#endif
