/*
=== INFO ===
Set a flag: nes->P |= flagHEX;
Clear a flag: nes->P &= ~flagHEX;
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "../includes/cpu.h"

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80


const char* opcodeTable[256] = {
    "BRK", "ORA", "???", "???", "???", "ORA", "ASL", "???", "PHP", "ORA", "ASL", "???", "???", "ORA", "ASL", "???",
    "BLP", "ORA", "???", "???", "???", "ORA", "ASL", "???", "CLC", "ORA", "???", "???", "???", "ORA", "ASL", "???",
};

void nes_init(CPU *nes) {
    memset(nes, 0, sizeof(CPU));

    nes->SP = 0xFD;

    memset(nes->gfx, 0, sizeof(nes->gfx)); // - Clear framebuffer

    nes->draw_flag = false;

    srand((unsigned) time(NULL));
}

int load_program(CPU *nes, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "❌ Cannot open file %s\n", filename);
        return 1;
    }

    uint8_t header[16];
    if (fread(header, 1, 16, file) != 16) {
        fprintf(stderr, "❌ Invalid NES header\n");
        fclose(file);
        return 1;
    }

    if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
        fprintf(stderr, "❌ Not a valid NES file\n");
        fclose(file);
        return 1;
    }

    uint8_t prg_size = header[4]; // nombre de PRG-ROM banks (16KB each)
    uint8_t chr_size = header[5]; // nombre de CHR-ROM banks (8KB each)

    if (prg_size * 16384 > sizeof(nes->prg_rom)) {
        fprintf(stderr, "❌ PRG-ROM too large\n");
        fclose(file);
        return 1;
    }

    // Lire PRG-ROM
    if (fread(nes->prg_rom, 1, prg_size * 16384, file) != prg_size * 16384) {
        fprintf(stderr, "❌ Failed to read PRG-ROM\n");
        fclose(file);
        return 1;
    }

    // Lire CHR-ROM si nécessaire
    if (chr_size > 0) {
        if (fread(nes->chr_rom, 1, chr_size * 8192, file) != chr_size * 8192) {
            fprintf(stderr, "❌ Failed to read CHR-ROM\n");
            fclose(file);
            return 1;
        }
    }

    fclose(file);

    // Init PC reset vector
    uint16_t reset_vector = nes->prg_rom[0x3FFC] | (nes->prg_rom[0x3FFD] << 8);
    nes->PC = reset_vector;

    printf("✅ ROM loaded (%d KB PRG, %d KB CHR). PC set to 0x%04X\n",
           prg_size * 16, chr_size * 8, nes->PC);

    return 0;
}

uint8_t nes_read(CPU *nes, uint16_t addr) {
    if (addr < 0x0800) return nes->ram[addr];         // RAM interne
    else if (addr >= 0x8000) return nes->prg_rom[addr - 0x8000]; // PRG-ROM
    // else : add PPU, IO etc
    return 0;
}

// LSR – Logical Shift Right
void cpu_lsr(CPU *nes, uint16_t addr, bool accumulator) {
    uint8_t value;

    if (accumulator) {
        value = nes->A;
    } else {
        value = nes_read(nes, addr);
    }

    if (value & 0x01)
        nes->P |= FLAG_C;
    else
        nes->P &= ~FLAG_C;

    value >>= 1;

    if (accumulator)
        nes->A = value;
    else
        nes->ram[addr] = value;

    if (value == 0)
        nes->P |= FLAG_Z;
    else
        nes->P &= ~FLAG_Z;

    nes->P &= ~FLAG_N; // N = 0 always after LSR
}

void cpu_dcp(CPU *nes, uint16_t addr) {
    nes->ram[addr] -= 1;
    uint8_t value = nes->ram[addr];

    // Compare with A
    uint16_t result = nes->A - value;

    if (result & 0x80) nes->P |= FLAG_N; else nes->P &= ~FLAG_N;
    if ((result & 0xFF) == 0) nes->P |= FLAG_Z; else nes->P &= ~FLAG_Z;
    if (nes->A >= value) nes->P |= FLAG_C; else nes->P &= ~FLAG_C;
}

void update_NZ_flags(CPU *nes, uint8_t value) {
    nes->P &= ~(FLAG_N | FLAG_Z);
    if (value == 0) nes->P |= FLAG_Z;
    if (value & 0x80) nes->P |= FLAG_N;
}


void nes_emulation_cycle(CPU *nes) {
    uint8_t opcode = nes->prg_rom[nes->PC++];
    const char* instruction = opcodeTable[opcode];

    switch (opcode) {
        case 0x00: // BRK - Force Interrupt
            printf("BRK at PC=0x%04X\n", nes->PC - 1);
            nes->PC++;
            break;

        


        case 0x07: { // SLO (zeropage)
            printf("SLO at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes->prg_rom[nes->PC++];
            uint8_t value = nes->ram[addr];

            if (value & 0x80) nes->P |= FLAG_C; else nes->P &= ~FLAG_C;
            value <<= 1;
            nes->ram[addr] = value;

            nes->A |= value;

            update_NZ_flags(nes, nes->A);
            break;
        }


        case 0x0C:  // NOP (immediate / absolute)
            printf("NOP at PC=0x%04X\n", nes->PC - 1);
            nes->PC += 2;
            break;

        
        case 0x10:
            printf("BPL at PC=0x%04X\n", nes->PC - 1);
            if ((nes->P & FLAG_N) == 0) {
                int8_t offset = nes_read(nes, nes->PC);
                nes->PC += 1 + offset;
            } else {
                nes->PC += 1;
            }
            break;

        case 0x13: { // SLO (Indirect),Y
            printf("SLO (Indirect),Y at PC=0x%04X\n", nes->PC - 1);

            uint8_t zp = nes->prg_rom[nes->PC++]; // adresse zéro-page
            uint16_t base = nes->ram[zp] | (nes->ram[(zp + 1) & 0xFF] << 8);
            uint16_t addr = base + nes->Y;

            uint8_t val = nes_read(nes, addr);

            // ASL : décalage à gauche, bit 7 -> Carry
            if (val & 0x80) nes->P |= FLAG_C; else nes->P &= ~FLAG_C;
            val <<= 1;

            // Stocker la valeur modifiée dans la RAM
            nes->ram[addr] = val;

            // ORA : A = A | M
            nes->A |= val;

            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0x16: { // ASL zeropage,X
            printf("ASL zeropage,X at PC=0x%04X\n", nes->PC - 1);

            uint8_t base = nes->prg_rom[nes->PC++];     // adresse zéro-page de base
            uint8_t addr = (base + nes->X) & 0xFF;      // ajout de X, reste sur 0x00-0xFF

            uint8_t val = nes->ram[addr];

            // Mettre à jour le Carry avec le bit 7
            if (val & 0x80) nes->P |= FLAG_C; 
            else nes->P &= ~FLAG_C;

            // Décalage à gauche
            val <<= 1;
            nes->ram[addr] = val;

            // Mettre à jour flags N et Z
            nes->P &= ~(FLAG_N | FLAG_Z);
            if (val == 0) nes->P |= FLAG_Z;
            if (val & 0x80) nes->P |= FLAG_N;

            break;
        }
        
        case 0x18:
            printf("CLC at PC=0x%04X\n", nes->PC - 1);
            nes->P &= ~FLAG_C;
            nes->PC++;
            break;
        
        case 0x20: {
            printf("JSR at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            uint16_t return_addr = nes->PC + 1;
            nes->stack[nes->SP--] = (return_addr >> 8) & 0xFF; // high
            nes->stack[nes->SP--] = return_addr & 0xFF; // low

            nes->PC = addr;
            break;
        }

        case 0x23: { // RLA (indirect,X)
            uint8_t zp = nes->prg_rom[nes->PC++];
            uint16_t ptr = (uint8_t)(zp + nes->X); // reste sur 0x00-0xFF
            uint16_t addr = nes->ram[ptr] | (nes->ram[(ptr+1)&0xFF] << 8);
            if (addr == 0x0000) {
                printf("⚠️ RLA pointer invalid, skipping\n");
                break; // ou juste return pour ne pas JAM
            }


            uint8_t value = nes_read(nes, addr);

            bool old_c = nes->P & FLAG_C;
            nes->P &= ~FLAG_C;
            if (value & 0x80) nes->P |= FLAG_C;

            value = (value << 1) | (old_c ? 1 : 0);
            nes->ram[addr] = value; // ⚠️ toujours écrire dans RAM/PRG-ROM mappée

            nes->A &= value;
            update_NZ_flags(nes, nes->A);

            printf("RLA: ZP=0x%02X, addr=0x%04X, val=0x%02X, A=0x%02X\n", zp, addr, value, nes->A);
            break;
        }

        case 0x24: { // BIT zeropage
            printf("BIT zeropage at PC=0x%04X\n", nes->PC - 1);

            uint8_t addr = nes->prg_rom[nes->PC++]; // lire 1 octet d'adresse
            uint8_t operand = nes_read(nes, addr);

            // Mettre à jour le flag Z (A & M)
            nes->P &= ~FLAG_Z;
            if ((nes->A & operand) == 0)
                nes->P |= FLAG_Z;

            // Mettre à jour N et V selon les bits 7 et 6 de M
            update_NZ_flags(nes, nes->A);

            break;
        }


        case 0x2C: { // BIT absolute
            printf("BIT at PC=0x%04X\n", nes->PC - 1);
            
            
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->PC += 2;

            uint8_t operand = nes_read(nes, addr);

            if ((nes->A & operand) == 0)
                nes->P |= FLAG_Z;
            else
                nes->P &= ~FLAG_Z;

            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0x30: { // BMI relative
            printf("BMI at PC=0x%04X\n", nes->PC - 1);

            int8_t offset = nes_read(nes, nes->PC++); // lire le décalage relatif

            if (nes->P & FLAG_N) {                   // si le flag N = 1
                nes->PC += offset;                   // saut relatif (peut être négatif)
            }

            break;
        }


        case 0x40: { // RTI - Return from Interrupt
            printf("RTI at PC=0x%04X\n", nes->PC - 1);

            // pull Status Register from stack
            nes->P = nes->stack[++nes->SP];

            // bit 5 is ignored (always set)
            nes->P |= FLAG_U;

            // pull Program Counter from stack (low byte first)
            uint8_t pcl = nes->stack[++nes->SP];
            uint8_t pch = nes->stack[++nes->SP];
            nes->PC = ((uint16_t)pch << 8) | pcl;

            break;
        }

        case 0x46: { // LSR $nn
            printf("LSR at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes->prg_rom[nes->PC++];
            cpu_lsr(nes, addr, false);
            break;
        }



        case 0x49: {
            printf("EOR at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->A = nes->A ^ nes->ram[addr];
            update_NZ_flags(nes, nes->A);
            nes->PC += 2;
            break;
        }

        case 0x60: {
            printf("RTS at PC=0x%04X\n", nes->PC - 1);

            uint8_t pcl = nes->stack[++nes->SP];  // pop low byte
            uint8_t pch = nes->stack[++nes->SP];  // pop high byte

            nes->PC = ((uint16_t)pch << 8) | pcl;
            nes->PC++;

            break;
        }

        case 0x63: { // RRA (indirect,X)
            uint8_t zp = nes->prg_rom[nes->PC++];
            uint16_t addr = nes->ram[(zp + nes->X) & 0xFF] | (nes->ram[(zp + nes->X + 1) & 0xFF] << 8);
            uint8_t value = nes_read(nes, addr);

            bool old_c = nes->P & FLAG_C;
            nes->P &= ~FLAG_C;
            if (value & 0x01) nes->P |= FLAG_C;

            value = (value >> 1) | (old_c ? 0x80 : 0x00);
            nes->ram[addr] = value;

            uint16_t sum = nes->A + value + (nes->P & FLAG_C);
            nes->A = sum & 0xFF;

            update_NZ_flags(nes, nes->A);
            if (sum > 0xFF) nes->P |= FLAG_C; else nes->P &= ~FLAG_C;
            break;
        }


        case 0x69: { // ADC immediate
            printf("ADC at PC=0x%04X\n", nes->PC - 1);

            uint8_t value = nes->prg_rom[nes->PC];
            uint16_t sum = nes->A + value + (nes->P & FLAG_C);

            nes->P &= ~(FLAG_C | FLAG_Z | FLAG_V | FLAG_N); // Clear C, Z, V, N
            if (sum > 0xFF) nes->P |= FLAG_C; // Carry
            if ((uint8_t)sum == 0) nes->P |= FLAG_Z; // Zero
            if (((nes->A ^ sum) & (value ^ sum) & FLAG_N) != 0) nes->P |= FLAG_V; // Overflow
            if (sum & FLAG_N) nes->P |= FLAG_N; // Negative

            nes->A = (uint8_t)sum;
            nes->PC += 1;
            break;
        }

        case 0x6B: { // ARR immediate
            printf("ARR #oper at PC=0x%04X\n", nes->PC - 1);

            uint8_t operand = nes->prg_rom[nes->PC++];
            nes->A &= operand;

            // Rotation à droite avec Carry
            bool old_c = nes->P & FLAG_C;
            nes->P &= ~(FLAG_C | FLAG_V | FLAG_Z | FLAG_N); // clear flags
            nes->A = (nes->A >> 1) | (old_c ? 0x80 : 0x00);

            // Mise à jour des flags
            if (nes->A == 0) nes->P |= FLAG_Z;          // Z
            if (nes->A & 0x80) nes->P |= FLAG_N;        // N
            if (nes->A & 0x40) nes->P |= FLAG_V;        // V = bit 6 après rotation
            if (nes->A & 0x01) nes->P |= FLAG_C;        // C = bit 0 après rotation

            break;
        }



        case 0x78:
            printf("SEI at PC=0x%04X\n", nes->PC - 1);
            nes->P |= FLAG_I;
            break;

        case 0x8D: {
            printf("STA at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->ram[addr] = nes->A;
            nes->PC += 2;
            break;
        }

        case 0x8E: {
            printf("STX at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->ram[addr] = nes->X;
            nes->PC += 2;
            break;
        }

        case 0x90: {
            printf("BCC at PC=0x%04X\n", nes->PC - 1);
            if ((nes->P & FLAG_C) == 0) { // if C = 0
                int8_t offset = nes_read(nes, nes->PC);
                nes->PC += 1 + offset;
            } else {
                nes->PC++;
            }
            break;
        }

        case 0x95: { // STA zeropage,X
            printf("STA zeropage,X at PC=0x%04X\n", nes->PC - 1);
            uint8_t zp_addr = nes->prg_rom[nes->PC++];
            uint8_t addr = (zp_addr + nes->X) & 0xFF; // reste sur zero page
            nes->ram[addr] = nes->A;
            break;
        }


        case 0x9A: { // TSX
            printf("TSX at PC=0x%04X\n", nes->PC - 1);
            nes->X = nes->SP; // SP -> X
            nes->P &= ~(FLAG_Z | FLAG_N);
            if (nes->X == 0) nes->P |= FLAG_Z;
            if (nes->X & FLAG_N) nes->P |= FLAG_N;
            nes->PC++;
            break;
        }

        
        case 0x9D: {
            printf("STA at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->ram[addr] = nes->A;
            nes->PC += 2;
            break;
        }

        case 0x9E: { // SHX absolute,Y
            printf("SHX absolute,Y at PC=0x%04X\n", nes->PC - 1);

            // Lire l'adresse absolue depuis le programme
            uint16_t base_addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->PC += 2;

            // Calculer l'adresse finale avec Y
            uint16_t addr = base_addr + nes->Y;

            // Valeur à stocker : X AND (high-byte de l'adresse + 1)
            uint8_t high_byte_plus_1 = ((addr >> 8) + 1) & 0xFF; // +1 comme le doc
            uint8_t value = nes->X & high_byte_plus_1;

            // Stocker dans la RAM (ou PRG-ROM mappée si tu veux simuler un write)
            nes->ram[addr] = value;

            break;
        }


        case 0xA1: { // LDA (Indirect,X)
            printf("LDA (Indirect,X) at PC=0x%04X\n", nes->PC - 1);

            uint8_t zp_addr = nes->prg_rom[nes->PC];
            nes->PC++;

            uint8_t addr_lo = nes->ram[(zp_addr + nes->X) & 0xFF];
            uint8_t addr_hi = nes->ram[(zp_addr + nes->X + 1) & 0xFF];
            uint16_t addr = addr_lo | (addr_hi << 8);


            nes->A = nes_read(nes, addr);

            // Update N, Z flags
            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0xA2: { // LDX immediate
            printf("LDX immediate at PC=0x%04X\n", nes->PC - 1);
            nes->X = nes->prg_rom[nes->PC]; // take next byte
            nes->PC += 1;
            
            nes->P &= ~(FLAG_Z | FLAG_N);         // Clear Z & N
            if (nes->X == 0) nes->P |= FLAG_Z;  // Set Z if 0
            if (nes->X & FLAG_N) nes->P |= FLAG_N; // Set N if bit 7
            break;
        }

        case 0xA3: {
            printf("LAX (indirect,X) at PC=0x%04X\n", nes->PC - 1);
            uint8_t zp = nes->prg_rom[nes->PC++];
            uint16_t addr = nes->ram[(zp + nes->X) & 0xFF] | (nes->ram[(zp + nes->X + 1) & 0xFF] << 8);
            uint8_t value = nes_read(nes, addr);
            nes->A = value;
            nes->X = value;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (value == 0) nes->P |= FLAG_Z;
            if (value & 0x80) nes->P |= FLAG_N;
            break;
        }

        case 0xA5: { // LDA zeropage
            printf("LDA zeropage at PC=0x%04X\n", nes->PC - 1);

            uint8_t addr = nes->prg_rom[nes->PC++];
            nes->A = nes->ram[addr];

            // Mettre à jour les flags N et Z
            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0xA7: {
            printf("LAX zeropage at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes->prg_rom[nes->PC++];
            uint8_t value = nes->ram[addr];
            nes->A = value;
            nes->X = value;
            update_NZ_flags(nes, nes->A);
            break;
        }
        
        case 0xA9: { // LDA immediate
            printf("LDA #$%02X at PC=0x%04X\n", nes->prg_rom[nes->PC], nes->PC - 1);
            nes->A = nes->prg_rom[nes->PC];
            nes->PC++;

            update_NZ_flags(nes, nes->A);
            break;
        }


        case 0xAD: {
            printf("LDA at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->A = nes_read(nes, addr);
            nes->PC += 2;
            break;
        }

        case 0xAF: {
            printf("LAX absolute at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->PC += 2;
            uint8_t value = nes_read(nes, addr);
            nes->A = value;
            nes->X = value;
            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0xB3: {
            printf("LAX (indirect),Y at PC=0x%04X\n", nes->PC - 1);
            uint8_t zp = nes->prg_rom[nes->PC++];
            uint16_t base = nes->ram[zp] | (nes->ram[(zp + 1) & 0xFF] << 8);
            uint16_t addr = base + nes->Y;
            uint8_t value = nes_read(nes, addr);
            nes->A = value;
            nes->X = value;
            update_NZ_flags(nes, nes->A);
            break;
        }

        // Zeropage,X - B5
        case 0xB5: { // LDA zeropage,X
            printf("LDA zeropage,X at PC=0x%04X\n", nes->PC - 1);

            uint8_t base = nes->prg_rom[nes->PC++];
            uint8_t addr = (base + nes->X) & 0xFF;
            nes->A = nes->ram[addr];                    // lire la valeur en mémoire

            // Mettre à jour flags N et Z
            update_NZ_flags(nes, nes->A);

            break;
        }


        case 0xB7: {
            printf("LAX zeropage,Y at PC=0x%04X\n", nes->PC - 1);
            uint8_t base = nes->prg_rom[nes->PC++];
            uint8_t addr = (base + nes->Y) & 0xFF;
            uint8_t value = nes->ram[addr];
            nes->A = value;
            nes->X = value;

            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0xBC: { // absolute, X
            printf("LDY absolute,X at PC=0x%04X\n", nes->PC - 1);

            // Lire l'adresse absolute (16 bits)
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->PC += 2;

            // Ajouter X pour obtenir l'adresse effective
            uint16_t effective_addr = addr + nes->X;
            nes->Y = nes_read(nes, effective_addr);

            // Mettre à jour les flags N et Z
            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->Y == 0) nes->P |= FLAG_Z;
            if (nes->Y & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0xBF: {
            printf("LAX absolute,Y at PC=0x%04X\n", nes->PC - 1);
            uint16_t base = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->PC += 2;
            uint16_t addr = base + nes->Y;
            uint8_t value = nes_read(nes, addr);
            nes->A = value;
            nes->X = value;

            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0xC3: {
            printf("DCP at PC=0x%04X\n", nes->PC - 1);
            uint8_t zp = nes->prg_rom[nes->PC++];
            uint16_t addr = nes->ram[(zp + nes->X) & 0xFF] | (nes->ram[(zp + nes->X + 1) & 0xFF] << 8);
            nes->ram[addr] -= 1;
            uint8_t value = nes->ram[addr];
            uint16_t result = nes->A - value;

            if (result & 0x80) nes->P |= FLAG_N; else nes->P &= ~FLAG_N;
            if ((result & 0xFF) == 0) nes->P |= FLAG_Z; else nes->P &= ~FLAG_Z;
            if (nes->A >= value) nes->P |= FLAG_C; else nes->P &= ~FLAG_C;
            break;
        }

        case 0xC6: { // DEC zeropage
            printf("DEC zeropage at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes->prg_rom[nes->PC]; // 1 bytes
            nes->PC += 1;

            nes->ram[addr] -= 1;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->ram[addr] == 0) nes->P |= FLAG_Z;
            if (nes->ram[addr] & 0x80) nes->P |= FLAG_N;

            break;
        }


        case 0xC9: { // CMP immediate
            uint8_t value = nes->prg_rom[nes->PC]; // get immediat operande
            nes->PC++;

            uint16_t result = nes->A - value;

            // -- Update Flags --
            if ((result & 0xFF) == 0)
                nes->P |= FLAG_Z;  // Set Z
            else
                nes->P &= ~FLAG_Z; // Clear Z

            if (nes->A >= value)
                nes->P |= FLAG_C;  // Set C
            else
                nes->P &= ~FLAG_C; // Clear C

            if (result & FLAG_N)
                nes->P |= FLAG_N;  // Set N
            else
                nes->P &= ~FLAG_N; // Clear N

            printf("CMP #$%02X at PC=0x%04X\n", value, nes->PC - 2);
            break;
        }

        case 0xCB: { // SBX immediate
            printf("SBX #oper at PC=0x%04X\n", nes->PC - 1);

            uint8_t operand = nes->prg_rom[nes->PC++];

            uint16_t result = (nes->A & nes->X) - operand;
            nes->X = result & 0xFF;

            // Mettre à jour les flags comme CMP
            nes->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
            if (nes->X == 0) nes->P |= FLAG_Z;           // Z
            if (nes->X & 0x80) nes->P |= FLAG_N;         // N
            if ((nes->A & nes->X) >= operand) nes->P |= FLAG_C; // C = A&X >= operand

            break;
        }

        case 0xCE: { // DEC absolute
            printf("DEC absolute at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            nes->PC += 2;

            nes->ram[addr] -= 1;
            uint8_t value = nes->ram[addr];

            // Mettre à jour les flags N et Z
            nes->P &= ~(FLAG_N | FLAG_Z);
            if (value == 0) nes->P |= FLAG_Z;
            if (value & 0x80) nes->P |= FLAG_N;

            break;
        }


        case 0xE6: {
            printf("INC at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes->prg_rom[nes->PC];
            nes->ram[addr] += 1;
            uint8_t value = nes->ram[addr];
            nes->P &= ~(FLAG_Z | FLAG_N);
            if (value == 0) nes->P |= FLAG_Z;
            if (value & FLAG_N) nes->P |= FLAG_N;
            nes->PC++;
            break;
        }

        case 0xE8:
            printf("INX at PC=0x%04X\n", nes->PC - 1);
            nes->X += 1;
            nes->P &= ~(FLAG_Z | FLAG_N);        // Clear Z and N
            if (nes->X == 0) nes->P |= FLAG_Z; // Set Z if zero
            if (nes->X & FLAG_N) nes->P |= FLAG_N; // Set N if bit 7
            nes->PC++;
            break;
        
        case 0xEA:
            printf("NOP at PC=0x%04X\n", nes->PC - 1);
            break;
        
        case 0xEE: {
            printf("INC (absolute) at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes->prg_rom[nes->PC] | (nes->prg_rom[nes->PC + 1] << 8);
            uint8_t value = ++nes->ram[addr];
            nes->P &= ~(FLAG_Z | FLAG_N);
            if (value == 0) nes->P |= FLAG_Z;  // Zero
            if (value & FLAG_N) nes->P |= FLAG_N; // Negative
            nes->PC += 2;
            break;
        }


        case 0xD8:
            printf("CLD at PC=0x%04X\n", nes->PC - 1);
            nes->P &= ~FLAG_D;
            break;

        case 0xF0: {
            printf("BEQ at PC=0x%04X\n", nes->PC - 1);
            int8_t offset = nes_read(nes, nes->PC);
            nes->PC++;
            if (nes->P & FLAG_Z)  // si flag Z = 1
                nes->PC += offset; // saute
            break;
        }

        case 0xF1: { // SBC ($nn),Y
            printf("SBC ($nn),Y at PC=0x%04X\n", nes->PC - 1);

            uint8_t zp_addr = nes->prg_rom[nes->PC++]; // adresse zéro-page
            // lire le pointeur 16 bits (adresse de base)
            uint16_t base = nes->ram[zp_addr] | (nes->ram[(zp_addr + 1) & 0xFF] << 8);
            uint16_t addr = base + nes->Y;

            uint8_t value = nes_read(nes, addr);

            // soustraction avec emprunt inverse : A - M - (1 - C)
            uint16_t result = nes->A - value - ((nes->P & 0x01) ? 0 : 1);

            // mise à jour du flag Carry (C = 1 si pas d'emprunt)
            if (result < 0x100)
                nes->P |= 0x01;
            else
                nes->P &= ~0x01;

            // mise à jour du flag Overflow (V)
            uint8_t overflow = ((nes->A ^ result) & 0x80) && ((nes->A ^ value) & 0x80);
            if (overflow)
                nes->P |= 0x40;
            else
                nes->P &= ~0x40;

            // mise à jour du flag Zero (Z)
            if ((result & 0xFF) == 0)
                nes->P |= 0x02;
            else
                nes->P &= ~0x02;

            // mise à jour du flag Negative (N)
            if (result & 0x80)
                nes->P |= 0x80;
            else
                nes->P &= ~0x80;

            nes->A = (uint8_t)result;
            break;
        }

        case 0xF5: { // SBC zeropage,X
            printf("SBC zeropage,X at PC=0x%04X\n", nes->PC - 1);

            uint8_t zp_addr = nes->prg_rom[nes->PC++];   // adresse de base (zéro-page)
            uint8_t addr = (zp_addr + nes->X) & 0xFF;    // ajout de X (reste sur 1 octet)
            uint8_t value = nes->ram[addr];              // valeur à soustraire

            // calcul : A - M - (1 - C)
            uint16_t result = nes->A - value - ((nes->P & 0x01) ? 0 : 1);

            // flag Carry : 1 si pas d’emprunt
            if (result < 0x100)
                nes->P |= 0x01;
            else
                nes->P &= ~0x01;

            // flag Overflow (débordement signé)
            uint8_t overflow = ((nes->A ^ result) & 0x80) && ((nes->A ^ value) & 0x80);
            if (overflow)
                nes->P |= 0x40;
            else
                nes->P &= ~0x40;

            // flag Zero
            if ((result & 0xFF) == 0)
                nes->P |= 0x02;
            else
                nes->P &= ~0x02;

            // flag Negative
            if (result & 0x80)
                nes->P |= 0x80;
            else
                nes->P &= ~0x80;

            nes->A = (uint8_t)result;
            break;
        }

        // Illegal opcode
        case 0x03:
        case 0x04:
        case 0x0F:
        case 0xF3:
        case 0xF4:
        case 0xF7:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFF:
            printf("🚫 Illegal opcode: %02X at PC=0x%04X\n", opcode, nes->PC - 1);
            nes->PC++;
            break;

        case 0x02: 
        case 0x12: 
        case 0x22: 
        case 0x32:
        case 0x42: 
        case 0x52: 
        case 0x62: 
        case 0x72:
        case 0x92: 
        case 0xB2: 
        case 0xD2: 
        case 0xF2:
            printf("💀 JAM (CPU halted) at PC=0x%04X\n", nes->PC - 1);
            printf("❌ CPU halted — reset required\n");
            exit(0); // ou une variable "cpu_halted = true;" si tu veux geler sans quitter
            break;
        
        default:
            printf("❌ Unimplemented opcode: %02X at PC=0x%04X\n", opcode, nes->PC - 1);
            exit(1);

    }

}