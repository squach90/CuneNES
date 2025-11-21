#include <stdint.h>
#include <string.h>
#include "../includes/cartridge.h"
#include "../includes/cpu.h"
#include "../includes/ppu.h"

// =======================
// Mapper 0 (NROM)
// =======================

void mapper0_load(Cartridge *cart, CPU *cpu, PPU *ppu) {
    // --- PRG-ROM ---
    if (cart->prg_count == 1) {
        // 16 KB : répéter deux fois pour $8000-$FFFF
        memcpy(cpu->prg_memory + 0x8000, cart->prg_rom, 0x4000);
        memcpy(cpu->prg_memory + 0xC000, cart->prg_rom, 0x4000);
        cpu->prg_banks = 1;
    } else if (cart->prg_count == 2) {
        // 32 KB : copier directement
        memcpy(cpu->prg_memory + 0x8000, cart->prg_rom, 0x8000);
        cpu->prg_banks = 2;
    }

    // --- CHR-ROM / CHR-RAM ---
    if (cart->chr_count > 0) {
        // Copier CHR-ROM dans la mémoire PPU
        memcpy(ppu->chr_rom, cart->chr_rom, cart->chr_count * 0x2000);
        ppu->chr_ram_enabled = false;
    } else {
        // CHR-RAM : allouer 8 KB
        memset(ppu->chr_rom, 0, 0x2000);
        ppu->chr_ram_enabled = true;
    }

    // --- Nametable mirroring ---
    // Mapper 0 ne fait pas de bank switching
    // On configure juste le type de mirroring pour le PPU
    // Horizontal = 0, Vertical = 1
    ppu->mirroring = cart->mirroring;
}
