#include <string.h>
#include <stdio.h>
#include "../includes/ppu.h"

static const uint32_t NES_PALETTE[64] = {
    0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400,
    0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
    0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058, 0xF83800, 0xE45C10,
    0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888, 0x000000, 0x000000, 0x000000,
    0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044,
    0xF8B800, 0xB8F818, 0x58D854, 0x58F898, 0x00E8D8, 0x787878, 0x000000, 0x000000,
    0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8,
    0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000, 0x000000
};

// === Init ===

void ppu_init(PPU *ppu) {
    memset(ppu, 0, sizeof(PPU));
    
    ppu->scanline = -1;  // Pre-render scanline
    ppu->cycle = 0;
    ppu->addr_latch = false;
    ppu->chr_ram_enabled = false;
    
    ppu->palette[0] = 0x0F;  // Black
    
    ppu->draw_flag = false;
    ppu->frame_count = 0;
    ppu->nmi_callback = NULL;

    // for (int y = 0; y < 240; y++) {
    //     for (int x = 0; x < 256; x++) {
    //         uint8_t color = ((x / 8) + (y / 8)) & 0x3F;
    //         ppu->framebuffer[y * 256 + x] = color;
    //     }
    // }

    ppu->palette[0] = 0x0F;
    ppu->palette[1] = 0xC5;  // Blanc
    ppu->palette[2] = 0x16;  // Rouge
    ppu->palette[3] = 0x27;  // Orange
}

void ppu_reset(PPU *ppu) {
    ppu->ctrl = 0;
    ppu->mask = 0;
    ppu->status = 0;
    ppu->oam_addr = 0;
    ppu->scroll_x = 0;
    ppu->scroll_y = 0;
    ppu->addr = 0;
    ppu->data = 0;
    ppu->addr_latch = false;
    ppu->data_buffer = 0;
    ppu->scanline = -1;
    ppu->cycle = 0;
}

// === PPU memory access ===

uint8_t ppu_read_memory(PPU *ppu, uint16_t addr) {
    addr &= 0x3FFF;  // Mirror
    
    // Pattern tables (0x0000-0x1FFF)
    if (addr < 0x2000) {
        return ppu->chr_rom[addr];
    }
    // Nametables (0x2000-0x2FFF)
    else if (addr < 0x3F00) {
        return ppu->vram[addr & 0x07FF];
    }
    // Palette (0x3F00-0x3FFF)
    else {
        uint16_t p = addr & 0x1F;
        // palette mirrors: 0x10/0x14/0x18/0x1C map to 0x00/0x04/0x08/0x0C
        if (p == 0x10) p = 0x00;
        if (p == 0x14) p = 0x04;
        if (p == 0x18) p = 0x08;
        if (p == 0x1C) p = 0x0C;
        return ppu->palette[p] & 0x3F;
    }
}

void ppu_write_memory(PPU *ppu, uint16_t addr, uint8_t value) {
    addr &= 0x3FFF;
    
    // Pattern tables (0x0000-0x1FFF)
    if (addr < 0x2000) {
        if (ppu->chr_ram_enabled) {
            ppu->chr_rom[addr] = value;  // CHR-RAM is writable
        }
        // CHR-ROM est read-only
    }
    // Nametables (0x2000-0x2FFF)
    else if (addr < 0x3F00) {
        printf("Writing to nametable: $%04X = $%02X\n", addr, value);
        ppu->vram[addr & 0x07FF] = value;
    }
    // Palette (0x3F00-0x3FFF)
    else {
        uint16_t p = addr & 0x1F;
        if (p == 0x10) p = 0x00;
        if (p == 0x14) p = 0x04;
        if (p == 0x18) p = 0x08;
        if (p == 0x1C) p = 0x0C;
        ppu->palette[p] = value & 0x3F;
    }
}

// === PPU Registres ===

void ppu_write_register(PPU *ppu, uint16_t addr, uint8_t value) {
    switch (addr & 0x2007) {
        case 0x2000:  // PPUCTRL
            ppu->ctrl = value;
            // Bits 0-1 : base nametable address
            ppu->temp_addr = (ppu->temp_addr & 0xF3FF) | ((value & 0x03) << 10);
            break;
            
        case 0x2001:  // PPUMASK
            ppu->mask = value;
            break;
            
        case 0x2003:  // OAMADDR
            ppu->oam_addr = value;
            break;
            
        case 0x2004:  // OAMDATA
            ppu->oam[ppu->oam_addr++] = value;
            break;
            
        case 0x2005:  // PPUSCROLL
            if (!ppu->addr_latch) {
                ppu->scroll_x = value;
                ppu->fine_x = value & 0x07;
                ppu->addr_latch = true;
            } else {
                ppu->scroll_y = value;
                ppu->addr_latch = false;
            }
            break;
            
        case 0x2006:  // PPUADDR
            if (!ppu->addr_latch) {
                ppu->addr = (ppu->addr & 0x00FF) | ((value & 0x3F) << 8); // High Byte
                ppu->addr_latch = true;
            } else {
                ppu->addr = (ppu->addr & 0xFF00) | value; // Low Byte
                ppu->addr_latch = false;
            }
            break;
            
        case 0x2007:  // PPUDATA
            ppu_write_memory(ppu, ppu->addr, value);
            if (ppu->ctrl & PPUCTRL_INCREMENT) {
                ppu->addr += 32;
            } else {
                ppu->addr += 1;
            }
            break;
    }
}

uint8_t ppu_read_register(PPU *ppu, uint16_t addr) {
    uint8_t value = 0;
    
    switch (addr & 0x2007) {
        case 0x2002:  // PPUSTATUS
            value = ppu->status;
            ppu->status &= ~PPUSTATUS_VBLANK;  // Clear VBlank flag
            ppu->addr_latch = false;            // Reset address latch
            break;
            
        case 0x2004:  // OAMDATA
            value = ppu->oam[ppu->oam_addr];
            break;
            
        case 0x2007:  // PPUDATA
            if (ppu->addr < 0x3F00) {
                value = ppu->data_buffer;
                ppu->data_buffer = ppu_read_memory(ppu, ppu->addr);
            } else {
                value = ppu_read_memory(ppu, ppu->addr);
                ppu->data_buffer = ppu_read_memory(ppu, ppu->addr - 0x1000);
            }
            
            if (ppu->ctrl & PPUCTRL_INCREMENT) {
                ppu->addr += 32;
            } else {
                ppu->addr += 1;
            }
            break;
            
        default:
            value = 0;
            break;
    }
    
    return value;
}

void ppu_get_tile_row(PPU *ppu, uint8_t tile_index, uint8_t row, uint8_t *pixels) {
    // Choose background pattern table base according to PPUCTRL bit $10
    uint16_t pattern_base = (ppu->ctrl & PPUCTRL_BG_PATTERN) ? 0x1000 : 0x0000;
    uint16_t addr = pattern_base + ((uint16_t)tile_index << 4) + (row & 0x07);

    uint8_t low_byte  = ppu->chr_rom[pattern_base + tile_index * 16 + row];
    uint8_t high_byte = ppu->chr_rom[pattern_base + tile_index * 16 + row + 8];

    for (int x = 0; x < 8; x++) {
        uint8_t bit_low  = (low_byte  >> (7 - x)) & 1;
        uint8_t bit_high = (high_byte >> (7 - x)) & 1;
        pixels[x] = (bit_high << 1) | bit_low;  // 0-3
    }
}

uint8_t ppu_get_nametable_tile(PPU *ppu, int tile_x, int tile_y) {
    // Nametable 0 start at $2000
    uint16_t nametable_addr = 0x2000;
    uint16_t offset = (tile_y % 30) * 32 + (tile_x % 32); // safe bounds
    return ppu_read_memory(ppu, nametable_addr + offset);
}

static uint8_t ppu_get_tile_palette_number(PPU *ppu, int tile_x, int tile_y) {
    // attribute table starts at ...0x23C0 for current nametable
    // attribute address calculation:
    uint16_t nametable_base = 0x2000; // currently we don't implement multiple nametable selection (use mirroring)
    uint16_t attrib_x = tile_x / 4;
    uint16_t attrib_y = tile_y / 4;
    uint16_t attrib_addr = 0x23C0 + (attrib_y * 8) + attrib_x;
    uint8_t attrib = ppu_read_memory(ppu, attrib_addr & 0x0FFF | 0x2000);

    // select quadrant within attribute byte
    int local_x = (tile_x & 0x03) / 2; // 0 or 1
    int local_y = (tile_y & 0x03) / 2; // 0 or 1
    int shift = (local_y * 2 + local_x) * 2;
    uint8_t palette = (attrib >> shift) & 0x03;

    return palette;
}

uint8_t ppu_get_background_color(PPU *ppu, uint8_t palette_num, uint8_t pixel_value) {
    // Pixel 0 = transparent
    if (pixel_value == 0) {
        return ppu->palette[0];
    }
    
    // Palette background : 4 colors per palette
    uint16_t palette_index = (palette_num << 2) | (pixel_value & 0x03);
    
    return ppu->palette[palette_index] & 0x3F;
}

void ppu_render_scanline(PPU *ppu) {
    if (ppu->scanline < 0 || ppu->scanline >= SCREEN_HEIGHT) return;
    int y = ppu->scanline;

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int tile_x = x / TILE_SIZE;
        int tile_y = y / TILE_SIZE;
        int pixel_x = x % TILE_SIZE;
        int pixel_y = y % TILE_SIZE;

        uint8_t tile_index = ppu_get_nametable_tile(ppu, tile_x, tile_y);
        uint8_t pixels[8];
        ppu_get_tile_row(ppu, tile_index, pixel_y, pixels);

        uint8_t palette_num = ppu_get_tile_palette_number(ppu, tile_x, tile_y);
        uint8_t pixel_value = pixels[pixel_x];

        uint8_t color_index = ppu_get_background_color(ppu, palette_num, pixel_value);

        uint8_t nes_color_index = ppu_get_background_color(ppu, palette_num, pixel_value);
        ppu->framebuffer[y * SCREEN_WIDTH + x] = 0xFF000000 | NES_PALETTE[nes_color_index];

    }
}

// === PPU Cycle ===

void ppu_step(PPU *ppu) {
    ppu->cycle++;
    
    // 341 cycles per scanline
    if (ppu->cycle > 340) {
        ppu->cycle = 0;
        ppu->scanline++;
        
        // 262 scanlines per frame (-1 à 260)
        if (ppu->scanline > 260) {
            ppu->scanline = -1;
            ppu->frame_count++;
        }
    }
    
    // Pre-render scanline (-1)
    if (ppu->scanline == -1) {
        if (ppu->cycle == 1) {
            ppu->status &= ~PPUSTATUS_VBLANK;
            ppu->status &= ~PPUSTATUS_SPRITE_0;
            ppu->status &= ~PPUSTATUS_SPRITE_OVERFLOW;
        }
    }
    
    // Scanlines visibles (0-239)
    if (ppu->scanline >= 0 && ppu->scanline < 240) {
        // TODO: render pixel per pixel
        if (ppu->cycle == 256) {
            ppu_render_scanline(ppu);
        }
    }
    
    // Post-render scanline (240)
    // Nothing to do :-)

    // VBlank scanlines (241-260)
    if (ppu->scanline == 241 && ppu->cycle == 1) {
        ppu->status |= PPUSTATUS_VBLANK;
        ppu->draw_flag = true;
        
        // Déclencher NMI si activé
        if ((ppu->ctrl & PPUCTRL_NMI_ENABLE) && ppu->nmi_callback) {
            ppu->nmi_callback();
        }
    }
}

// === Callbacks ===

void ppu_set_nmi_callback(PPU *ppu, void (*callback)(void)) {
    ppu->nmi_callback = callback;
}

// === Utils ===

void ppu_dump_palette(PPU *ppu) {
    printf("=== Palette PPU ===\n");
    printf("Background palettes:\n");
    for (int i = 0; i < 16; i++) {
        if (i % 4 == 0) printf("  Palette %d: ", i / 4);
        printf("$%02X ", ppu->palette[i]);
        if (i % 4 == 3) printf("\n");
    }
    printf("Sprite palettes:\n");
    for (int i = 16; i < 32; i++) {
        if (i % 4 == 0) printf("  Palette %d: ", (i - 16) / 4);
        printf("$%02X ", ppu->palette[i]);
        if (i % 4 == 3) printf("\n");
    }
}

void ppu_dump_oam(PPU *ppu) {
    printf("=== OAM (Sprites) ===\n");
    for (int i = 0; i < 64; i++) {
        uint8_t y = ppu->oam[i * 4 + 0];
        uint8_t tile = ppu->oam[i * 4 + 1];
        uint8_t attr = ppu->oam[i * 4 + 2];
        uint8_t x = ppu->oam[i * 4 + 3];
        
        if (y < 0xEF) {
            printf("Sprite %02d: Y=%3d X=%3d Tile=$%02X Attr=$%02X\n",
                   i, y, x, tile, attr);
        }
    }
}