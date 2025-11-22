#ifndef MAPPER0_H
#define MAPPER0_H

#include "cpu.h"
#include "ppu.h"
#include "cartridge.h"
#include "mapper.h"

// Création du mapper 0
Mapper *mapper0_create(Cartridge *cart);
void mapper0_load(Cartridge *cart, CPU *cpu, PPU *ppu);


#endif // MAPPER0_H
