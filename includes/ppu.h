#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240

#define TILE_WIDTH 8
#define TILE_HEIGHT 8

#define VRAM_SIZE 0x4000      // 16 KB VRAM (PPU $0000-$3FFF)
#define PALETTE_SIZE 32       // Colors (background + sprites)
#define OAM_SIZE 256          // Object Attribute Memory (sprites, 64x4 octets)

typedef struct {
    uint8_t PPUCTRL;    // $2000
    uint8_t PPUMASK;    // $2001
    uint8_t PPUSTATUS;  // $2002
    uint8_t OAMADDR;    // $2003
    uint8_t OAMDATA;    // $2004
    uint8_t PPUSCROLL;  // $2005
    uint8_t PPUADDR;    // $2006
    uint8_t PPUDATA;    // $2007
} PPU_Registers;


typedef struct {
    PPU_Registers regs;

    uint8_t vram[VRAM_SIZE];
    uint8_t palette[PALETTE_SIZE];
    uint8_t oam[OAM_SIZE];

    uint8_t screen[SCREEN_HEIGHT][SCREEN_WIDTH]; // Framebuffer
    bool frame_complete; // indicate the frame can be "print"
} PPU;

#endif
