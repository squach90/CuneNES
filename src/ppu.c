#include "../includes/ppu.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void ppu_init(PPU *ppu) {
    memset(ppu, 0, sizeof(PPU));
    ppu->v = 0;
    ppu->t = 0;
    ppu->x = 0;
    ppu->w = 0;
    ppu->status = 0xA0;
    
    ppu->palette[0] = 0x0F;
    ppu->palette[1] = 0x16;
    ppu->palette[2] = 0x27;
    ppu->palette[3] = 0x30;
    
    printf("✅ PPU initialized\n");
}

void ppu_tick(PPU* ppu) {
    ppu->cycle++;

    if (ppu->cycle >= 341) {
        ppu->cycle = 0;
        ppu->scanline++;

        if (ppu->scanline == 241) {
            ppu->status |= 0x80;  // Set bit 7 (VBlank)
            ppu->frame_complete = true;
        }

        if (ppu->scanline >= 262) {
            ppu->scanline = 0;
            ppu->status &= 0x7F;  // Clear bit 7
        }
    }
}

uint8_t ppu_read(PPU *ppu, uint16_t addr) {
    addr &= 0x3FFF;
    
    if (addr >= 0x3F00) {
        return ppu->palette[addr & 0x1F];
    } else if (addr >= 0x2000 && addr < 0x3000) {
        return ppu->vram[0x2000 + (addr & 0x03FF)];
    } else {
        return ppu->vram[addr & 0x1FFF];
    }
}

void ppu_write(PPU *ppu, uint16_t addr, uint8_t value) {
    addr &= 0x3FFF;
    
    if (addr >= 0x3F00) {
        ppu->palette[addr & 0x1F] = value;
    } else {
        ppu->vram[addr] = value;
    }
}

void ppu_render_tile(PPU* ppu, int tile_x, int tile_y) {
    uint16_t nametable_addr = 0x2000 + tile_y * 32 + tile_x;
    uint8_t tile_index = ppu->vram[nametable_addr];

    uint16_t pattern_table_addr = tile_index * 16;

    for (int y = 0; y < 8; y++) {
        uint8_t byte1 = ppu->vram[pattern_table_addr + y];
        uint8_t byte2 = ppu->vram[pattern_table_addr + y + 8];

        for (int x = 0; x < 8; x++) {
            int bit0 = (byte1 >> (7 - x)) & 1;
            int bit1 = (byte2 >> (7 - x)) & 1;
            int color_index = bit1 * 2 + bit0;
            
            uint8_t palette_color = ppu->palette[color_index];
            
            int screen_x = tile_x * 8 + x;
            int screen_y = tile_y * 8 + y;
            
            if (screen_x < SCREEN_WIDTH && screen_y < SCREEN_HEIGHT) {
                ppu->screen[screen_y][screen_x] = palette_color;
            }
        }
    }
}