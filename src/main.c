#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include "../includes/cpu.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <ROM file>\n", argv[0]);
        return 1;
    }

    const char *rom_path = argv[1];

    CPU nes;
    nes_init(&nes);

    if (load_program(&nes, rom_path) != 0) {
        fprintf(stderr, "❌ Failed to load ROM: %s\n", rom_path);
        return 1;
    }

    printf("✅ ROM loaded successfully. PC at 0x%04X\n", nes.PC);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
        nes_emulation_cycle(&nes);
    }

    return 0;
}
