#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "ppu.h"

typedef struct {
    // == Memory ==
    uint8_t ram[2048];      // RAM (2 KB)
    uint8_t prg_rom[32768]; // PRG-ROM (max 32 KB)
    uint8_t chr_rom[8192];  // CHR-ROM (graphismes)

    // == CPU Registers ==
    uint8_t A;   // Accumulateur
    uint8_t X;   // Registre X
    uint8_t Y;   // Registre Y
    uint8_t P;   // Status flags (N, V, B, D, I, Z, C)  = 0x80, 0x40 ???, 0x08, 0x04, 0x02, 0x01
    uint16_t PC; // Program Counter
    uint8_t SP;  // Stack Pointer

    uint16_t stack[256];

    uint8_t gfx[256 * 240]; // Framebuffer

    uint8_t key[8]; // 8 buttons : A, B, Select, Start, Up, Down, Left, Right
    
    bool draw_flag;
    PPU *ppu;
} CPU;

void nes_init(CPU *nes);
int load_program(CPU *nes, PPU *ppu, const char *filename);
void nes_write(CPU *nes, uint16_t addr, uint8_t value);
void nes_emulation_cycle(CPU *nes);

uint8_t nes_read(CPU *nes, uint16_t addr);

#endif
