#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "ppu.h"

typedef struct {
    // == Memory ==
    uint8_t ram[2048];      // RAM (2 KB)
    uint8_t prg_rom[32768]; // PRG-ROM (max 32 KB)

    uint8_t prg_banks;

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

    PPU *ppu;
    uint64_t cycles;
    bool nmi_pending;
    
    bool draw_flag;
} CPU;

void nes_init(CPU *nes);
void cpu_connect_ppu(CPU *cpu, PPU *ppu);

int load_program(CPU *nes, const char *filename);
void nes_write(CPU *nes, uint16_t addr, uint8_t value);
uint8_t nes_read(CPU *nes, uint16_t addr);

void nes_emulation_cycle(CPU *nes);

void cpu_nmi(CPU *cpu);

#endif
