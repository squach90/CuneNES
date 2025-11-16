#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

#define TILE_SIZE 8 // 8x8

typedef struct {
    uint8_t chr_rom[8192];  // CHR-ROM (Pattern Memory) 0x0000 - 0x1FFF (Sprites)
    uint8_t vram[2048];  // VRAM (Name Table Memory) 0x2000 - 0x3EFF (Layout)
    uint8_t paletteMem[256];  // Palette Memory 0x3F00 - 0x3FFF (Colors)

} PPU;

#endif
