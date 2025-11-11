#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../includes/cpu.h"

// Déclarations des fonctions que tu vas créer
void nes_init(CPU *nes);
int load_program(CPU *nes, const char *filename);

int main(int argc, char **argv) {
    // == Init ==
    CPU nes;
    nes_init(&nes);

    const char *file_path = "roms/DonkeyKong.nes";

    if (load_program(&nes, file_path) != 0) {
        fprintf(stderr, "❌ Failed to load ROM.\n");
        return 1;
    }

    printf("✅ ROM loaded successfully. PC at 0x%04X\n", nes.PC);

    while (1) {
        nes_emulation_cycle(&nes);
    }
    

    // Ici tu lanceras la boucle principale NES (fetch-decode-execute)
    // par exemple : while (running) { nes_execute(&nes); ... }

    return 0;
}
