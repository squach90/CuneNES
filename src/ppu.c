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

void ppu_init(PPU *ppu) {
    memset(ppu, 0 sizeof(PPU));

    ppu->scanline = -1; // Pre-render scanline
    ppu->cycle = 0;
    ppu->chr_ram_enabled = false;
    ppu->addr_latch = false;
    
    ppu->paletteMem[0] = 0x0F; // Black

    ppu->draw_flag = false;
    ppu->frame_count = 0;
    ppu->nmi_callback = false;

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
    ppu->scanline = -1; // Pre-render scanline
    ppu->cycle = 0;
}

uint8_t ppu_read_memory(PPU *ppu, uint16_t addr) {
    addr &= 0x3FFF; // Mirror (PPU mem -> max 16Kb)

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
        addr &= 0x1F;

        if (addr == 0x10) addr = 0x00;
        if (addr == 0x14) addr = 0x04;
        if (addr == 0x18) addr = 0x08;
        if (addr == 0x1C) addr = 0x0C;
        return ppu->palette[addr];
    }
}

uint8_t ppu_write_memory(PPU *ppu, uint16_t addr, uint8_t value) {
    addr &= 0x3FFF; // Mirror (PPU mem -> max 16Kb)

    // Pattern tables (0x0000-0x1FFF)
    if (addr < 0x2000) {
        if (ppu->chr_ram_enabled) {
            ppu->chr_rom[addr] = value;
        }
    }

    // Nametables (0x2000-0x2FFF)
    else if (addr < 0x3F00) {
        ppu->vram[addr & 0x07FF] = value;
    }

    // Palette (0x3F00-0x3FFF)
    else {
        addr &= 0x1F;

        if (addr == 0x10) addr = 0x00;
        if (addr == 0x14) addr = 0x04;
        if (addr == 0x18) addr = 0x08;
        if (addr == 0x1C) addr = 0x0C;
        ppu->palette[addr] = value & 0x3F; // Only 6 bits is use

    }
}

uint8_t ppu_write_register(PPU *ppu, uint16_t addr, uint8_t value) {
    switch (addr & 0x2007) {
    case 0x2000: // PPUCTRL
        ppu->ctrl = value;
        ppu->temp_addr = (ppu->temp_addr & 0xF3FF) | ((value & 0x03) << 10);
        break;
    
    case 0x2001: // PPUMASK
        ppu->mask = value;
        break;

    case 0x2003: // OAMADDR
        ppu->oam_addr = value;
        break;
    
    case 0x2004:  // OAMDATA
        ppu->oam[ppu->oam_addr++] = value;
        break;
    
    case 0x2005:  // PPUSCROLL
        if (!ppu->addr_latch) {
            // Premier write : X scroll
            ppu->scroll_x = value;
            ppu->fine_x = value & 0x07;
            ppu->addr_latch = true;
        } else {
            // Deuxième write : Y scroll
            ppu->scroll_y = value;
            ppu->addr_latch = false;
        }
        break;
        
    case 0x2006:  // PPUADDR
        if (!ppu->addr_latch) {
            // Premier write : high byte
            ppu->addr = (ppu->addr & 0x00FF) | ((value & 0x3F) << 8);
            ppu->addr_latch = true;
        } else {
            // Deuxième write : low byte
            ppu->addr = (ppu->addr & 0xFF00) | value;
            ppu->addr_latch = false;
        }
        break;
        
    case 0x2007:  // PPUDATA
        ppu_write_memory(ppu, ppu->addr, value);
        // Incrémenter l'adresse
        if (ppu->ctrl & PPUCTRL_INCREMENT) {
            ppu->addr += 32;  // Mode vertical
        } else {
            ppu->addr += 1;   // Mode horizontal
        }
        break;
    
    default:
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

void ppu_step(PPU *ppu) {
    ppu->cycle++; // Move of 1 pixel/dot

    if (ppu->cycle > 340){
        ppu->cycle = 0;
        ppu->scanline++;
    }

    if (ppu->scanline > 260) {
        ppu->scanline = -1;
        ppu->frame_count++;
    }

    if (ppu->scanline == -1){
        if (ppu->cycle == 1) {
            ppu->status &= ~PPUSTATUS_VBLANK; // Clear VBlank flag
            ppu->status &= ~PPUSTATUS_SPRITE_0; // Clear Sprite 0 hit
            ppu->status &= ~PPUSTATUS_SPRITE_OVERFLOW;
        }
    }

    if (ppu->scanline >= 0 && ppu->scanline < 240) {
        // TODO: Render pixel by pixel
    }

    if (ppu->scanline == 241 && ppu->cycle == 1){
        ppu->status |= PPUSTATUS_VBLANK;  // Set VBlank flag
        ppu->draw_flag = true; // Frame ready to be draw
        
        if ((ppu->ctrl & PPUCTRL_NMI_ENABLE) && ppu->nmi_callback) {
            ppu->nmi_callback(); // Call CPU
        }
    }
}