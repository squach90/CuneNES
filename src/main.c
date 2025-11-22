#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL.h>
#include "../includes/cpu.h"
#include "../includes/ppu.h"
#include "../includes/cartridge.h"
#include "../includes/mapper.h"


#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240
#define SCALE_FACTOR 3  // Agrandir l'écran x3 (768x720)

// Palette NES complète (64 couleurs)
static const uint32_t NES_PALETTE[64] = {
    0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400,
    0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
    0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058, 0xF83800, 0xE45C10,
    0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888, 0x000000, 0x000000, 0x000000,
    0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044,
    0xF8B800, 0xB8F818, 0x58D854, 0x58F898, 0x00E8D8, 0x787878, 0x000000, 0x000000,
    0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8,
    0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000, 0x000000
};

// Variables globales pour SDL
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
} Display;

// Callback NMI appelé par le PPU
static CPU *global_cpu = NULL;

void nmi_callback(void) {
    if (global_cpu) {
        cpu_nmi(global_cpu);
    }
}

// Initialiser l'affichage SDL
int init_display(Display *display) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "❌ SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    display->window = SDL_CreateWindow(
        "NES Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * SCALE_FACTOR,
        SCREEN_HEIGHT * SCALE_FACTOR,
        SDL_WINDOW_SHOWN
    );

    if (!display->window) {
        fprintf(stderr, "❌ SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    display->renderer = SDL_CreateRenderer(
        display->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!display->renderer) {
        fprintf(stderr, "❌ SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(display->window);
        SDL_Quit();
        return 1;
    }

    display->texture = SDL_CreateTexture(
        display->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (!display->texture) {
        fprintf(stderr, "❌ SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(display->renderer);
        SDL_DestroyWindow(display->window);
        SDL_Quit();
        return 1;
    }

    printf("✅ SDL initialized successfully\n");
    return 0;
}

// Nettoyer SDL
void cleanup_display(Display *display) {
    if (display->texture) SDL_DestroyTexture(display->texture);
    if (display->renderer) SDL_DestroyRenderer(display->renderer);
    if (display->window) SDL_DestroyWindow(display->window);
    SDL_Quit();
}

void render_frame(Display *display, PPU *ppu) {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        uint8_t nes_color_index = ppu->framebuffer[i];       // valeur 0-63
        display->pixels[i] = NES_PALETTE[ppu->framebuffer[i]];   // couleur RGB réelle
    }

    SDL_UpdateTexture(display->texture, NULL, display->pixels, 
                     SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(display->renderer);
    SDL_RenderCopy(display->renderer, display->texture, NULL, NULL);
    SDL_RenderPresent(display->renderer);
}


void handle_input(SDL_Event *event, CPU *cpu, bool *running) {
    while (SDL_PollEvent(event)) {
        if (event->type == SDL_QUIT) {
            *running = false;
        }
        
        if (event->type == SDL_KEYDOWN) {
            switch (event->key.keysym.sym) {
                case SDLK_ESCAPE:  *running = false; break;
                case SDLK_x:       cpu->key[0] = 1; break; // A
                case SDLK_z:       cpu->key[1] = 1; break; // B
                case SDLK_RSHIFT:  cpu->key[2] = 1; break; // Select
                case SDLK_RETURN:  cpu->key[3] = 1; break; // Start
                case SDLK_UP:      cpu->key[4] = 1; break; // Up
                case SDLK_DOWN:    cpu->key[5] = 1; break; // Down
                case SDLK_LEFT:    cpu->key[6] = 1; break; // Left
                case SDLK_RIGHT:   cpu->key[7] = 1; break; // Right
            }
        }
        
        if (event->type == SDL_KEYUP) {
            switch (event->key.keysym.sym) {
                case SDLK_x:       cpu->key[0] = 0; break;
                case SDLK_z:       cpu->key[1] = 0; break;
                case SDLK_RSHIFT:  cpu->key[2] = 0; break;
                case SDLK_RETURN:  cpu->key[3] = 0; break;
                case SDLK_UP:      cpu->key[4] = 0; break;
                case SDLK_DOWN:    cpu->key[5] = 0; break;
                case SDLK_LEFT:    cpu->key[6] = 0; break;
                case SDLK_RIGHT:   cpu->key[7] = 0; break;
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <ROM file>\n", argv[0]);
        printf("Controls:\n");
        printf("  Arrow keys : D-Pad\n");
        printf("  Z          : B button\n");
        printf("  X          : A button\n");
        printf("  Enter      : Start\n");
        printf("  Right Shift: Select\n");
        printf("  ESC        : Quit\n");
        return 1;
    }

    const char *rom_path = argv[1];

    // === Init CPU, PPU & CART ===
    CPU cpu;
    PPU ppu;
    Cartridge *cart = cartridge_load(rom_path);
    
    nes_init(&cpu);
    ppu_init(&ppu);
    
    cart->mapper->load(cart, &cpu, &ppu);
    cpu_connect_ppu(&cpu, &ppu);
    

    global_cpu = &cpu;
    ppu_set_nmi_callback(&ppu, nmi_callback);

    // === Load ROM ===
    if (!cart) {
        fprintf(stderr, "❌ Failed to load cartridge: %s\n", rom_path);
        return 1;
    }

    cpu_load_cartridge(&cpu, cart);
    // ppu_load_cartridge(&ppu, cart);
    cpu_nmi(&cpu);

    printf("✅ ROM loaded successfully. PC at 0x%04X\n", cpu.PC);

    printf("PPU: first nametable tile at $2000: %02X\n", nes_read(&cpu, 0x2000));
    printf("PPU: first CHR-ROM tile: %02X\n", ppu.chr_rom[0]);

    // === Init SDL ===
    Display display = {0};
    if (init_display(&display) != 0) {
        return 1;
    }

    // === Main loop ===
    bool running = true;
    SDL_Event event;
    
    printf("✅ Emulator started. Press ESC to quit.\n");
    printf("Reset vector: %02X %02X\n", cpu.prg_memory[0xFFFC], cpu.prg_memory[0xFFFD]);
    printf("PRG memory at $8000: %02X %02X %02X %02X\n",
       cpu.prg_memory[0x8000],
       cpu.prg_memory[0x8001],
       cpu.prg_memory[0x8002],
       cpu.prg_memory[0x8003]);


    while (running) {
        handle_input(&event, &cpu, &running);

        nes_emulation_cycle(&cpu);
        for (int i = 0; i < 89342; i++)
            ppu_step(&ppu);

        if (ppu.draw_flag) {
            printf("PPU framebuffer first pixels: %02X %02X %02X %02X\n",
                ppu.framebuffer[0], ppu.framebuffer[1],
                ppu.framebuffer[2], ppu.framebuffer[3]);


            render_frame(&display, &ppu);
            ppu.draw_flag = false;
        }
    }

    cleanup_display(&display);
    printf("✅ Emulator closed properly\n");
    
    return 0;
}