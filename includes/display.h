#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include "../includes/ppu.h"

int display_init(PPU *ppu);
void display_loop(PPU *ppu);
void display_cleanup(void);

#endif
