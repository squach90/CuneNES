#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_WIDTH  256
#define SCREEN_HEIGHT 240

#define TILE_SIZE 8 // 8x8

#define PPU_PATTERN_TABLE_0  0x0000  // Pattern table 0 (sprites)
#define PPU_PATTERN_TABLE_1  0x1000  // Pattern table 1 (background)
#define PPU_NAMETABLE_0      0x2000
#define PPU_NAMETABLE_1      0x2400
#define PPU_NAMETABLE_2      0x2800
#define PPU_NAMETABLE_3      0x2C00
#define PPU_PALETTE_START    0x3F00

#define PPUCTRL   0x2000
#define PPUMASK   0x2001
#define PPUSTATUS 0x2002
#define OAMADDR   0x2003
#define OAMDATA   0x2004
#define PPUSCROLL 0x2005
#define PPUADDR   0x2006
#define PPUDATA   0x2007

// Flags PPUCTRL ($2000)
#define PPUCTRL_NMI_ENABLE    0x80
#define PPUCTRL_SPRITE_SIZE   0x20
#define PPUCTRL_BG_PATTERN    0x10
#define PPUCTRL_SPRITE_PATTERN 0x08
#define PPUCTRL_INCREMENT     0x04

// Flags PPUMASK ($2001)
#define PPUMASK_SHOW_BG       0x08
#define PPUMASK_SHOW_SPRITES  0x10
#define PPUMASK_SHOW_LEFT_BG  0x02
#define PPUMASK_SHOW_LEFT_SPR 0x04

// Flags PPUSTATUS ($2002)
#define PPUSTATUS_VBLANK      0x80
#define PPUSTATUS_SPRITE_0    0x40
#define PPUSTATUS_SPRITE_OVERFLOW 0x20

typedef struct {
    uint8_t chr_rom[8192];  // CHR-ROM (Pattern Memory) 0x0000 - 0x1FFF (Sprites)
    bool chr_ram_enabled;

    uint8_t vram[2048];  // VRAM (Name Table Memory) 0x2000 - 0x27FF (Layout)
    uint8_t paletteMem[32];  // Palette Memory 0x3F00 - 0x3F1F (Colors)
    uint8_t oam[256];  // Object Attribute Memory (sprites)

    // === Registres PPU ($2000-$2007) ===
    uint8_t ctrl;               // $2000 - PPUCTRL
    uint8_t mask;               // $2001 - PPUMASK
    uint8_t status;             // $2002 - PPUSTATUS
    uint8_t oam_addr;           // $2003 - OAMADDR
    int8_t scroll_x;            // $2005 - PPUSCROLL X
    uint8_t scroll_y;           // $2005 - PPUSCROLL Y
    uint16_t addr;              // $2006 - PPUADDR (16-bit)
    uint8_t data;               // $2007 - PPUDATA

    bool addr_latch;
    uint8_t data_buffer;
    uint16_t temp_addr;
    uint8_t fine_x;

    // === Framebuffer ===
    uint8_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool draw_flag;

    // === State ===
    int scanline;  // Actual line (0-261)
    int cycle;  // Scanline Cycle (0-340)

    uint64_t frame_count;

    void (*nmi_callback)(void);

} PPU;

// === Init Func ===
void ppu_init(PPU *ppu);
void ppu_reset(PPU *ppu);

// === Register Access ===
void ppu_write_register(PPU *ppu, uint16_t addr, uint8_t value);
uint8_t ppu_read_register(PPU *ppu, uint16_t addr);

// === Memory Access ===
void ppu_write_memory(PPU *ppu, uint16_t addr, uint8_t value);
uint8_t ppu_read_memory(PPU *ppu, uint16_t addr);

// === PPU Cycle ===
void ppu_step(PPU *ppu);        // Exécuter un cycle PPU

// === Callbacks NMI ===
void ppu_set_nmi_callback(PPU *ppu, void (*callback)(void));

// === Debug ===
void ppu_dump_palette(PPU *ppu);
void ppu_dump_oam(PPU *ppu);

#endif
