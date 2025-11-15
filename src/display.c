// display.c
#include <SDL.h>
#include <stdint.h>
#include "../includes/ppu.h"
#include "../includes/display.h"

#define WINDOW_SCALE 3

// Palette de couleurs NES
uint32_t nes_palette[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4,
    0x5C007E, 0x6E0040, 0x6C0700, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08,
    0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE,
    0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0E9300, 0x008F32,
    0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF,
    0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082,
    0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF,
    0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC,
    0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000
};

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;

int display_init(PPU *ppu) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init Error: %s", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow(
        "NES Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * WINDOW_SCALE,
        SCREEN_HEIGHT * WINDOW_SCALE,
        SDL_WINDOW_SHOWN
    );
    if (!window) return 1;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return 1;
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );
    if (!texture) return 1;
    return 0;
}

void display_loop(PPU* ppu) {
    uint32_t pixels[SCREEN_HEIGHT][SCREEN_WIDTH];
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint8_t color_index = ppu->screen[y][x] & 0x3F; // 6 bits
            pixels[y][x] = nes_palette[color_index];
        }
    }
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void display_cleanup() {
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}
