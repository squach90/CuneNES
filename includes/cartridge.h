#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// -------------------------------
// NES Mirroring Modes
// -------------------------------
typedef enum {
    MIRROR_HORIZONTAL = 0,
    MIRROR_VERTICAL   = 1,
    MIRROR_FOURSCREEN = 2
} MirrorMode;

struct Mapper; // Forward declaration
typedef struct Mapper Mapper;

// -------------------------------
// Cartridge structure
// -------------------------------
typedef struct {
    uint8_t prg_count;   // Number of 16KB PRG ROM banks
    uint8_t chr_count;   // Number of 8KB CHR ROM banks

    uint8_t *prg_rom;    // Allocated dynamically
    uint8_t *chr_rom;        // CHR-ROM or CHR-RAM

    bool chr_is_ram;     // True if CHR = RAM

    uint8_t mapper_id;   // Mapper number (0 = NROM, 1 = MMC1, ...)

    MirrorMode mirroring;

    Mapper *mapper;      // Pointer to mapper implementation
} Cartridge;

// -------------------------------
// Cartridge loading / freeing
// -------------------------------
Cartridge *cartridge_load(const char *filename);
void cartridge_free(Cartridge *cart);

#endif // CARTRIDGE_H
