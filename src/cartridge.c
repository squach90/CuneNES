#include "../includes/cartridge.h"
#include "../includes/mapper0.h"
#include <stdlib.h>
#include <string.h>

// Forward declarations of mappers
Mapper *mapper0_create(Cartridge *cart);
Mapper *mapper1_create(Cartridge *cart);

Cartridge *cartridge_load(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "❌ Cannot open ROM %s\n", filename);
        return NULL;
    }

    uint8_t header[16];
    if (fread(header, 1, 16, file) != 16) {
        fprintf(stderr, "❌ Invalid NES header\n");
        fclose(file);
        return NULL;
    }

    if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
        fprintf(stderr, "❌ Not a valid iNES file\n");
        fclose(file);
        return NULL;
    }

    Cartridge *cart = calloc(1, sizeof(Cartridge));

    cart->prg_count = header[4];
    cart->chr_count = header[5];
    cart->mapper_id = (header[6] >> 4) | (header[7] & 0xF0);

    printf("📀 PRG banks : %d (×16 KB)\n", cart->prg_count);
    printf("🎨 CHR banks : %d (×8 KB)\n", cart->chr_count);
    printf("🗺️ Mapper ID : %d\n", cart->mapper_id);

    // Mirroring
    if (header[6] & 1)
        cart->mirroring = MIRROR_VERTICAL;
    else
        cart->mirroring = MIRROR_HORIZONTAL;

    int trainer = (header[6] & 0x04) ? 512 : 0;
    fseek(file, 16 + trainer, SEEK_SET);

    // --------- Load PRG-ROM ---------
    size_t prg_size = cart->prg_count * 16384;
    cart->prg_rom = malloc(prg_size);
    fread(cart->prg_rom, 1, prg_size, file);

    // --------- Load CHR-ROM or allocate CHR-RAM ---------
    if (cart->chr_count > 0) {
        size_t chr_size = cart->chr_count * 8192;
        cart->chr_rom = malloc(chr_size);
        fread(cart->chr_rom, 1, chr_size, file);
        cart->chr_is_ram = false;
    } else {
        cart->chr_rom = calloc(1, 8192); // CHR-RAM
        cart->chr_is_ram = true;
        printf("📝 Using CHR-RAM (8 KB)\n");
    }

    fclose(file);

    // --------- Create mapper ---------
    switch (cart->mapper_id) {
        case 0: 
            cart->mapper = mapper0_create(cart);
            break;
        case 1: cart->mapper = NULL; break;
        default:
            printf("❌ Mapper %d not supported\n", cart->mapper_id);
            free(cart->prg_rom);
            free(cart->chr_rom);
            free(cart);
            return NULL;
    }

    printf("✅ Cartridge loaded successfully\n");
    return cart;
}

void cartridge_free(Cartridge *cart) {
    if (!cart) return;

    free(cart->prg_rom);
    free(cart->chr_rom);
    free(cart->mapper->data);
    free(cart->mapper);
    free(cart);
}
