#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>
#include "cartridge.h"
#include "cpu.h"
#include "ppu.h"

typedef void (*MapperLoadFunc)(Cartridge *cart, CPU *cpu, PPU *ppu);

typedef struct Mapper {
    uint8_t (*read)(struct Mapper *mapper, uint16_t addr);
    void    (*write)(struct Mapper *mapper, uint16_t addr, uint8_t value);
    MapperLoadFunc load;
    int mapper_id;
    void *data;
} Mapper;

#endif
