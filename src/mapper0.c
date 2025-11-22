#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../includes/mapper.h"
#include "../includes/cartridge.h"
#include "../includes/cpu.h"
#include "../includes/ppu.h"


// =======================
// Mapper 0 (NROM)
// =======================

#include <stdint.h>
#include <string.h>
#include "../includes/cartridge.h"
#include "../includes/cpu.h"
#include "../includes/ppu.h"

// =======================
// Mapper 0 (NROM)
// =======================

void mapper0_load(Cartridge *cart, CPU *cpu, PPU *ppu) {
    if (cart->prg_count == 1) {
        // répéter 16 KB 2 fois
        memcpy(cpu->prg_memory + 0x8000, cart->prg_rom, 0x4000);
        memcpy(cpu->prg_memory + 0xC000, cart->prg_rom, 0x4000);
        cpu->prg_banks = 1;
    } else if (cart->prg_count == 2) {
        memcpy(cpu->prg_memory + 0x8000, cart->prg_rom, 0x8000);
        cpu->prg_banks = 2;
    }

    // CHR-ROM / RAM
    if (cart->chr_count > 0)
        memcpy(ppu->chr_rom, cart->chr_rom, cart->chr_count * 0x2000);
    else
        memset(ppu->chr_rom, 0, 0x2000);

    // Nametable mirroring
    ppu->mirroring = cart->mirroring;

    // Set CPU PC
    cpu->PC = cpu->prg_memory[0xFFFC] | (cpu->prg_memory[0xFFFD] << 8);
    
}



uint8_t mapper0_read(Mapper *m, uint16_t addr) {
    Cartridge *cart = m->data;

    if (addr >= 0x8000) {
        uint32_t offset = addr - 0x8000;

        if (cart->prg_count == 1)
            offset %= 0x4000; // mirroring 16 KB

        return cart->prg_rom[offset];
    }

    return 0;
}

void mapper0_write(Mapper *m, uint16_t addr, uint8_t value) {
    Cartridge *cart = m->data;

    if (cart->chr_is_ram && addr < 0x2000) {
        cart->chr_rom[addr] = value;
    }
}

Mapper *mapper0_create(Cartridge *cart) {
    Mapper *m = calloc(1, sizeof(Mapper));

    m->read = mapper0_read;
    m->write = mapper0_write;
    m->load = mapper0_load;
    m->data = cart;
    return m;
}