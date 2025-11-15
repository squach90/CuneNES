#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include "../includes/cpu.h"
#include "../includes/ppu.h"
#include "../includes/display.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <ROM file>\n", argv[0]);
        return 1;
    }

    const char *rom_path = argv[1];

    CPU nes;
    nes_init(&nes);

    
    PPU ppu;
    ppu_init(&ppu);

    nes.ppu = &ppu;
    
    if (load_program(&nes, &ppu, rom_path) != 0) {
        fprintf(stderr, "❌ Failed to load ROM: %s\n", rom_path);
        return 1;
    }

    printf("✅ ROM loaded successfully. PC at 0x%04X\n", nes.PC);

    printf("Nametable sample: ");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", ppu.vram[0x2000 + i]);
    }
    printf("\n");
    
    // Afficher le contenu du CHR-ROM pour debug
    printf("CHR-ROM sample: ");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", ppu.vram[i]);
    }
    printf("\n");
    
    // Initialise SDL et passe le PPU à display
    if (display_init(&ppu) != 0) {
        fprintf(stderr, "❌ Failed to initialize display\n");
        return 1;
    }

    bool running = true;
    SDL_Event event;

    int frame_count = 0;
    int cpu_cycles_per_frame = 29781;  // ~29781 cycles CPU par frame (60 FPS)

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        for (int i = 0; i < cpu_cycles_per_frame; i++) {
            // 1 cycle CPU
            nes_emulation_cycle(&nes);

            // 3 cycles PPU par cycle CPU
            for (int j = 0; j < 3; j++) {
                ppu_tick(&ppu);
            }
            
            // Si une frame est complète, on rend l'écran
            if (ppu.frame_complete) {
                // Rendre toutes les tuiles
                for (int ty = 0; ty < 30; ty++) {
                    for (int tx = 0; tx < 32; tx++) {
                        ppu_render_tile(&ppu, tx, ty);
                    }
                }
                
                // Afficher à l'écran
                display_loop(&ppu);
                
                ppu.frame_complete = false;
                frame_count++;
                
                // Debug : afficher toutes les 60 frames
                if (frame_count % 60 == 0) {
                    printf("Frame %d rendered\n", frame_count);
                }
                
                // Limiter à ~60 FPS
                SDL_Delay(16);
                
                break;  // Sortir de la boucle pour traiter les événements SDL
            }
        }

    }
    
    display_cleanup();

    return 0;
}
