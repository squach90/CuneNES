#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../includes/cpu.h"

void nes_init(CPU *nes);
int load_program(CPU *nes, const char *filename);

int main(int argc, char **argv) {
    // == Init ==
    CPU nes;
    nes_init(&nes);

    const char *file_path = "roms/Mario.nes";

    if (load_program(&nes, file_path) != 0) {
        fprintf(stderr, "❌ Failed to load ROM.\n");
        return 1;
    }

    printf("✅ ROM loaded successfully. PC at 0x%04X\n", nes.PC);

    while (1) {
        nes_emulation_cycle(&nes);
    }

    return 0;
}
