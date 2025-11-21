#ifndef MAPPER0_H
#define MAPPER0_H

#include "cpu.h"
#include "ppu.h"
#include "cartridge.h"

// Fonction pour charger un cartouche avec mapper 0
void mapper0_load(Cartridge *cart, CPU *cpu, PPU *ppu);

#endif // MAPPER0_H
