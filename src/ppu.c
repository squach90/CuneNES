#include <string.h>
#include <stdio.h>
#include "../includes/ppu.h"
#include "../includes/cartridge.h"

static const uint32_t NES_PALETTE[64] = {
    0x545454,0x001E74,0x081090,0x300088,0x440064,0x5C002E,0x540400,0x3C1800,
    0x201A00,0x005A00,0x006400,0x005C48,0x005454,0x000000,0x000000,0x000000,
    0x989698,0x084CCC,0x3032EC,0x5C1EE4,0x8814B0,0xA01464,0x981E20,0x783C00,
    0x545A00,0x287200,0x005024,0x005864,0x0030A0,0x787878,0x000000,0x000000,
    0xECEEEC,0x4C9AEC,0x787CEC,0xB062EC,0xE454EC,0xEC58B4,0xEC6A64,0xD48820,
    0xA0AA00,0x74C400,0x4CD020,0x38CC6C,0x38B4CC,0x3C3C3C,0x000000,0x000000,
    0xECEEEC,0xA8CCEC,0xBCBCEC,0xD4B2EC,0xECB0EC,0xECB0D4,0xECB4B0,0xE4C490,
    0xCCD478,0xB4DE78,0xA8E290,0x98E2B4,0xA0D6E4,0xA0A2A0,0x000000,0x000000
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

    ppu->vram_addr = 0;
    ppu->fine_x = 0;

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
        uint16_t nt_index = (addr >> 10) & 0x03; // 0..3
        uint16_t local_addr = addr & 0x03FF;

        if (ppu->mirroring == MIRROR_HORIZONTAL) {
            // NT0 = $2000/$2400, NT1 = $2800/$2C00
            if (nt_index <= 1) return ppu->vram[local_addr];       // NT0
            else return ppu->vram[local_addr + 0x400];           // NT1
        } else if (ppu->mirroring == MIRROR_VERTICAL) {
            // NT0 = $2000/$2800, NT1 = $2400/$2C00
            if (nt_index == 0 || nt_index == 2) return ppu->vram[local_addr];   // NT0
            else return ppu->vram[local_addr + 0x400];                          // NT1
        }
    }
    // Palette (0x3F00-0x3FFF)
    else {
        uint16_t p = addr & 0x1F;
        if (p == 0x10) p = 0x00;
        if (p == 0x14) p = 0x04;
        if (p == 0x18) p = 0x08;
        if (p == 0x1C) p = 0x0C;
        return ppu->palette[p] & 0x3F;
    }

    return 0;
}


void ppu_write_memory(PPU *ppu, uint16_t addr, uint8_t value) {
    addr &= 0x3FFF;

    // Pattern tables (0x0000-0x1FFF)
    if (addr < 0x2000) {
        if (ppu->chr_ram_enabled) {
            ppu->chr_rom[addr] = value;  // CHR-RAM writable
        }
    }
    // Nametables (0x2000-0x2FFF)
    else if (addr < 0x3F00) {
        uint16_t nt_index = (addr >> 10) & 0x03; // 0..3
        uint16_t local_addr = addr & 0x03FF;

        if (ppu->mirroring == MIRROR_HORIZONTAL) {
            if (nt_index <= 1) ppu->vram[local_addr] = value;         // NT0
            else ppu->vram[local_addr + 0x400] = value;              // NT1
        } else if (ppu->mirroring == MIRROR_VERTICAL) {
            if (nt_index == 0 || nt_index == 2) ppu->vram[local_addr] = value; // NT0
            else ppu->vram[local_addr + 0x400] = value;                      // NT1
        }
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

void ppu_load_cartridge(PPU *ppu, Cartridge *cart) {
    memcpy(ppu->chr_rom, cart->chr_rom, sizeof(ppu->chr_rom));

    // Activate CHR-RAM if no ROM
    ppu->chr_ram_enabled = true;
    for (size_t i = 0; i < sizeof(cart->chr_rom); i++) {
        if (cart->chr_rom[i] != 0) {
            ppu->chr_ram_enabled = false;
            break;
        }
    }
}


void ppu_get_tile_row(PPU *ppu, uint8_t tile_index, uint8_t row, uint8_t *pixels) {
    // Choose background pattern table base according to PPUCTRL bit $10
    uint16_t pattern_base = (ppu->ctrl & PPUCTRL_BG_PATTERN) ? 0x1000 : 0x0000;
    uint16_t addr = pattern_base + (tile_index * 16);

    uint8_t low_byte  = ppu->chr_rom[addr + row];
    uint8_t high_byte = ppu->chr_rom[addr + row + 8];


    for (int x = 0; x < 8; x++) {
        uint8_t bit_low  = (low_byte  >> (7 - x)) & 1;
        uint8_t bit_high = (high_byte >> (7 - x)) & 1;
        pixels[x] = (bit_high << 1) | bit_low;  // 0-3
    }
}

uint8_t ppu_get_nametable_tile(PPU *ppu, int abs_tile_x, int abs_tile_y)
{
    int local_x = abs_tile_x % 32;
    int local_y = abs_tile_y % 30;

    uint16_t addr = 0x2000 + (local_y * 32) + local_x;

    if (ppu->mirroring == MIRROR_HORIZONTAL)
        addr = 0x2000 + ((local_y * 32 + local_x) & 0x07FF); // 2 NT
    else if (ppu->mirroring == MIRROR_VERTICAL)
        addr = 0x2000 + ((local_y * 32 + local_x) & 0x03FF) | ((local_y / 30) ? 0x400 : 0);

    return ppu_read_memory(ppu, addr);
}


uint8_t ppu_get_tile_palette_number(PPU *ppu, int abs_tile_x, int abs_tile_y) {
    int local_x = abs_tile_x % 32;
    int local_y = abs_tile_y % 30;
    
    int nt_x = (abs_tile_x / 32) % 2;
    int nt_y = (abs_tile_y / 30) % 2;
    int nt_index = nt_y * 2 + nt_x;

    uint16_t base_addr = 0x2000 + (nt_index * 0x400);
    
    uint16_t attrib_addr = base_addr + 0x3C0 + (local_y / 4) * 8 + (local_x / 4);
    
    uint8_t attrib = ppu_read_memory(ppu, attrib_addr);
    
    int quadrant_x = (local_x % 4) / 2;
    int quadrant_y = (local_y % 4) / 2;
    int shift = (quadrant_y * 2 + quadrant_x) * 2;
    
    return (attrib >> shift) & 0x03;
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

void ppu_render_pixel(PPU *ppu, int x, int y) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;

    uint8_t base_nt_idx = ppu->ctrl & 0x03;
    int base_x = (base_nt_idx & 1) * 256;
    int base_y = ((base_nt_idx >> 1) & 1) * 240;

    int scrolled_x = x + ppu->scroll_x + base_x;
    int scrolled_y = y + ppu->scroll_y + base_y;
    scrolled_x %= 512; // 2 NT horizontalement
    scrolled_y %= 480; // 2 NT verticalement


    int abs_tile_x = scrolled_x / 8;
    int abs_tile_y = scrolled_y / 8;
    
    int pixel_x = scrolled_x % 8;
    int pixel_y = scrolled_y % 8;

    uint8_t tile_index = ppu_get_nametable_tile(ppu, abs_tile_x, abs_tile_y);
    uint8_t palette_num = ppu_get_tile_palette_number(ppu, abs_tile_x, abs_tile_y);
    
    uint8_t pixels[8];
    ppu_get_tile_row(ppu, tile_index, pixel_y, pixels);
    
    uint8_t pixel_value = pixels[pixel_x];
    uint8_t nes_color_index = ppu_get_background_color(ppu, palette_num, pixel_value);
    
    ppu->framebuffer[y * SCREEN_WIDTH + x] = nes_color_index;
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
        if (ppu->cycle > 0 && ppu->cycle <= 256) {
            ppu_render_pixel(ppu, ppu->cycle - 1, ppu->scanline);
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