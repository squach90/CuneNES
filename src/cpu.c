/*
=== INFO ===
Set a flag: nes->P |= flagHEX;
Clear a flag: nes->P &= ~flagHEX;

=== TYPE ===
Immediate: nes->ram[nes->PC++];

Zero page: nes->ram[nes->PC++];

Zeropage,X: uint8_t base = nes->ram[nes->PC++];
            uint8_t addr = (base + nes->X) & 0xFF;

Zeropage,Y: uint8_t base = nes->ram[nes->PC++];
            uint8_t addr = (base + nes->Y) & 0xFF;

Absolute:   uint16_t addr = nes->ram[nes->PC] | (nes->ram[nes->PC + 1] << 8);
            nes->PC += 2;

Absolute,X: uint16_t base = nes->ram[nes->PC] | (nes->ram[nes->PC + 1] << 8);
            nes->PC += 2;
            uint16_t addr = base + nes->X;

Absolute,Y: uint16_t base = nes->ram[nes->PC] | (nes->ram[nes->PC + 1] << 8);
            nes->PC += 2;
            uint16_t addr = base + nes->Y;

Indirect: uint16_t ptr = nes->ram[nes->PC] | (nes->ram[nes->PC + 1] << 8);
          nes->PC += 2;
          uint16_t addr = nes->ram[ptr] | (nes->ram[ptr + 1] << 8);

Indexed indirect ($nn,X): uint8_t zp = nes->ram[nes->PC++];
                          uint16_t addr = nes->ram[(zp + nes->X) & 0xFF] | (nes->ram[(zp + nes->X + 1) & 0xFF] << 8);

Indirect indexed ($nn),Y: uint8_t zp = nes->ram[nes->PC++];
                          uint16_t base = nes->ram[zp] | (nes->ram[(zp + 1) & 0xFF] << 8);
                          uint16_t addr = base + nes->Y;

Relative (branch): int8_t offset = nes->ram[nes->PC++];
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "../includes/cpu.h"
#include "../includes/cartridge.h"
#include "../includes/mapper0.h"

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

#define DEBUG_CPU 0

void nes_init(CPU *nes) {
    memset(nes, 0, sizeof(CPU));
    nes->SP = 0xFD;
    memset(nes->gfx, 0, sizeof(nes->gfx));
    nes->draw_flag = false;
    srand((unsigned) time(NULL));
}

void cpu_connect_ppu(CPU *cpu, PPU *ppu) {
    cpu->ppu = ppu;
}

void nes_write(CPU *nes, uint16_t addr, uint8_t value) {
    // $0000-$07FF : RAM
    if (addr < 0x0800) {
        nes->ram[addr] = value;
    }
    // $0800-$1FFF : Miroirs de la RAM
    else if (addr < 0x2000) {
        nes->ram[addr & 0x07FF] = value;
    }
    // $2000-$2007 : Registres PPU
    else if (addr >= 0x2000 && addr < 0x2008) {
        if (nes->ppu) {
            ppu_write_register(nes->ppu, addr, value);
        }
    }
    // $2008-$3FFF : Miroirs des registres PPU
    else if (addr < 0x4000) {
        if (nes->ppu) {
            ppu_write_register(nes->ppu, 0x2000 + (addr & 0x0007), value);
        }
    }
    // $4000-$4013, $4015, $4017 : APU
    else if (addr < 0x4020) {
        // TODO: APU
    }
    // $8000-$FFFF : ROM (read-only)
}


uint8_t nes_read(CPU *nes, uint16_t addr) {
    // $0000-$07FF : RAM (2KB)
    if (addr < 0x0800) {
        return nes->ram[addr];
    }
    // $0800-$1FFF : Miroirs de la RAM
    else if (addr < 0x2000) {
        return nes->ram[addr & 0x07FF];
    }
    // $2000-$2007 : Registres PPU
    else if (addr >= 0x2000 && addr < 0x2008) {
        if (nes->ppu) {
            return ppu_read_register(nes->ppu, addr);
        }
        return 0;
    }
    // $2008-$3FFF : Miroirs des registres PPU
    else if (addr < 0x4000) {
        if (nes->ppu) {
            return ppu_read_register(nes->ppu, 0x2000 + (addr & 0x0007));
        }
        return 0;
    }
    // $4000-$4017 : APU et I/O
    else if (addr < 0x4020) {
        return 0;  // TODO
    }
    // $8000-$FFFF : PRG-ROM
    else if (addr >= 0x8000) {
        if (nes->prg_banks == 1)
            return nes->prg_memory[addr & 0x3FFF];  // mirror bank
        else if (nes->prg_banks == 2)
            return nes->prg_memory[addr - 0x8000];  // bank0/$8000-$BFFF, bank1/$C000-$FFFF
    }

    
    return 0;
}

void cpu_nmi(CPU *cpu) {
    // Push PC (high byte puis low byte)
    cpu->ram[0x0100 + cpu->SP--] = (cpu->PC >> 8) & 0xFF;
    cpu->ram[0x0100 + cpu->SP--] = cpu->PC & 0xFF;
    
    // Push P
    cpu->ram[0x0100 + cpu->SP--] = cpu->P & ~0x10;
    
    // Set I flag
    cpu->P |= 0x04;
    
    // Jump to NMI vector($FFFA-$FFFB)
    uint16_t nmi_vector = nes_read(cpu, 0xFFFA) | (nes_read(cpu, 0xFFFB) << 8);
    cpu->PC = nmi_vector;
    
    cpu->cycles += 7;
    
    printf("NMI triggered! Jumping to $%04X\n", nmi_vector);
}

void cpu_load_cartridge(CPU *cpu, Cartridge *cart) {
    if (cart->mapper_id == 0) {
        // Charge la ROM et les CHR dans le CPU/PPU
        mapper0_load(cart, cpu, cpu->ppu);

        // Affiche les 16 premiers octets de la CHR-ROM
        printf("First 16 bytes of CHR-ROM:\n");
        for (int i = 0; i < 16; i++) 
            printf("%02X ", cpu->ppu->chr_rom[i]);
        printf("\n");

        // Affiche les 16 premiers pixels du framebuffer
        printf("First 16 pixels of framebuffer:\n");
        for (int i = 0; i < 16; i++) 
            printf("%02X ", cpu->ppu->framebuffer[i]);
        printf("\n");
    }

    // Initialisation du PC selon la taille de la ROM
    if (cpu->prg_banks == 1) {
        // Une seule banque PRG-ROM de 16 KB : mirror pour $C000-$FFFF
        cpu->PC = cpu->prg_memory[0x3FFC] | (cpu->prg_memory[0x3FFD] << 8);
    } else if (cpu->prg_banks == 2) {
        // Deux banques PRG-ROM : vector dans $FFFC-$FFFD
        cpu->PC = nes_read(cpu, 0xFFFC) | (nes_read(cpu, 0xFFFD) << 8);
    }

    printf("✅ CPU reset vector set to 0x%04X\n", cpu->PC);
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
    uint8_t opcode = nes_read(nes, nes->PC++);

    switch (opcode) {
        case 0x00: // BRK - Force Interrupt
            printf("BRK at PC=0x%04X\n", nes->PC - 1);
            nes->PC++;
            break;

        case 0x01: { // ORA (Indirect,X) - OR with memory (Indirect,X)
            uint8_t zp = nes_read(nes, nes->PC++);
            uint16_t addr = nes_read(nes, (zp + nes->X) & 0xFF) |
                        (nes_read(nes, (zp + nes->X + 1) & 0xFF) << 8);
            uint8_t value = nes_read(nes, addr);
            printf("ORA (Indirect,X) at PC=0x%04X, ZP=0x%02X, X=0x%02X, addr=0x%04X, value=0x%02X, A=0x%02X\n",
                nes->PC - 1, zp, nes->X, addr, value, nes->A);
            nes->A |= value;
            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0x03: { // SLO (indirect,X)
            printf("SLO (indirect,X) at PC=0x%04X\n", nes->PC - 1);

            uint8_t zp = nes_read(nes, nes->PC++);
            uint8_t ptr_low = nes->ram[(zp + nes->X) & 0xFF];
            uint8_t ptr_high = nes->ram[(zp + nes->X + 1) & 0xFF];
            uint16_t addr = ptr_low | (ptr_high << 8);

            uint8_t m = nes->ram[addr];

            // --- ASL M ---
            if (m & 0x80) nes->P |= FLAG_C;
            else nes->P &= ~FLAG_C;

            m <<= 1;
            nes->ram[addr] = m;

            // --- ORA M ---
            nes->A |= m;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->A == 0) nes->P |= FLAG_Z;
            if (nes->A & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0x04: { // NOP zeropage
            printf("NOP zeropage at PC=0x%04X\n", nes->PC - 1);
            nes->PC++;
            break;
        }


        case 0x06: { // ASL zeropage
            printf("ASL zeropage at PC=0x%04X\n", nes->PC - 1);

            uint8_t addr = nes_read(nes, nes->PC++);

            uint8_t val = nes->ram[addr];

            
            if (val & 0x80) nes->P |= FLAG_C; 
            else nes->P &= ~FLAG_C;

            // bitshift on the left
            val <<= 1;
            nes->ram[addr] = val;

            update_NZ_flags(nes, val);

            break;
        }

        case 0x07: { // SLO (zeropage)
            printf("SLO at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes_read(nes, nes->PC++);
            uint8_t value = nes->ram[addr];

            if (value & 0x80) nes->P |= FLAG_C; else nes->P &= ~FLAG_C;
            value <<= 1;
            nes->ram[addr] = value;

            nes->A |= value;

            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0x08: { // PHP (Push Processor Status)
            printf("PHP at PC=0x%04X\n", nes->PC - 1);

            uint8_t status = nes->P | 0x30;

            nes->ram[0x0100 + nes->SP] = status;
            nes->SP--;

            break;
        }


        case 0x0A: { // ASL accumulator
            printf("ASL A at PC=0x%04X\n", nes->PC - 1);

            uint8_t old = nes->A;

            if (old & 0x80) 
                nes->P |= FLAG_C;
            else 
                nes->P &= ~FLAG_C;

            nes->A <<= 1;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->A == 0) nes->P |= FLAG_Z;
            if (nes->A & 0x80) nes->P |= FLAG_N;

            nes->PC++;
            break;
        }


        case 0x0C:  // NOP (immediate / absolute)
            printf("NOP at PC=0x%04X\n", nes->PC - 1);
            nes->PC += 2;
            break;
        
        case 0x0D: { // ORA absolute
            printf("ORA absolute at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint8_t value = nes_read(nes, addr);

            nes->A |= value;

            nes->P &= ~(FLAG_Z | FLAG_N);
            if (nes->A == 0) nes->P |= FLAG_Z;
            if (nes->A & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0x0F: {  // SLO / ASO zeropage,X
            printf("SLO (zeropage,X) at PC=0x%04X\n", nes->PC - 1);

            uint8_t base = nes_read(nes, nes->PC++);
            uint8_t addr = (base + nes->X) & 0xFF;

            // ASL M
            uint8_t m = nes->ram[addr];
            if (m & 0x80) nes->P |= FLAG_C;
            else nes->P &= ~FLAG_C;

            m <<= 1;
            nes->ram[addr] = m;

            // ORA M -> A
            nes->A |= m;

            // Flags N et Z
            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->A & 0x80) nes->P |= FLAG_N;
            if (nes->A == 0) nes->P |= FLAG_Z;

            break;
        }


        
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

            uint8_t zp = nes_read(nes, nes->PC++);
            uint16_t base = nes->ram[zp] | (nes->ram[(zp + 1) & 0xFF] << 8);
            uint16_t addr = base + nes->Y;

            uint8_t val = nes_read(nes, addr);

            if (val & 0x80) nes->P |= FLAG_C; else nes->P &= ~FLAG_C;
            val <<= 1;

            nes->ram[addr] = val;

            // ORA : A = A | M
            nes->A |= val;

            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0x16: { // ASL zeropage,X
            printf("ASL zeropage,X at PC=0x%04X\n", nes->PC - 1);

            uint8_t base = nes_read(nes, nes->PC++);
            uint8_t addr = (base + nes->X) & 0xFF;

            uint8_t val = nes->ram[addr];

            if (val & 0x80) nes->P |= FLAG_C; 
            else nes->P &= ~FLAG_C;

            val <<= 1;
            nes->ram[addr] = val;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (val == 0) nes->P |= FLAG_Z;
            if (val & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0x17: { // ORA (Indirect),Y
            printf("ORA (Indirect),Y at PC=0x%04X\n", nes->PC - 1);
            uint8_t zp = nes_read(nes, nes->PC);
            nes->PC += 1;

            uint16_t base = nes_read(nes, zp) | (nes_read(nes, (uint8_t)(zp + 1)) << 8);
            uint16_t addr = base + nes->Y;

            uint8_t value = nes_read(nes, addr);
            nes->A |= value;

            update_NZ_flags(nes, nes->A);
            break;
        }

        
        case 0x18:
            printf("CLC at PC=0x%04X\n", nes->PC - 1);
            nes->P &= ~FLAG_C;
            nes->PC++;
            break;

        case 0x19: { // ORA absolute,Y
            printf("ORA absolute,Y at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint16_t effective_addr = addr + nes->Y;

            uint8_t value = nes_read(nes, effective_addr);

            nes->A |= value;

            nes->P &= ~(FLAG_Z | FLAG_N);
            if (nes->A == 0) nes->P |= FLAG_Z;
            if (nes->A & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0x1D: { // ORA absolute,X
            printf("ORA absolute,X at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint16_t effective_addr = addr + nes->X;

            uint8_t value = nes_read(nes, effective_addr);

            nes->A |= value;

            nes->P &= ~(FLAG_Z | FLAG_N);
            if (nes->A == 0) nes->P |= FLAG_Z;
            if (nes->A & 0x80) nes->P |= FLAG_N;

            break;
        }

        
        case 0x20: { // JSR absolute
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            uint16_t return_addr = nes->PC + 1;
            nes->ram[0x0100 + nes->SP--] = (return_addr >> 8) & 0xFF;
            nes->ram[0x0100 + nes->SP--] = return_addr & 0xFF;
            nes->PC = addr;
            break;
        }

        case 0x21: { // AND (Indirect,X)
            printf("AND (Indirect,X) at PC=0x%04X\n", nes->PC - 1);

            uint8_t oper = nes_read(nes, nes->PC++);
            uint8_t zp_addr = (oper + nes->X) & 0xFF;

            uint16_t addr = nes->ram[zp_addr] | (nes->ram[(zp_addr + 1) & 0xFF] << 8);

            uint8_t value = nes_read(nes, addr);
            nes->A &= value;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if(nes->A == 0) nes->P |= FLAG_Z;
            if(nes->A & 0x80) nes->P |= FLAG_N;

            break;
        }


        case 0x23: { // RLA (indirect,X)
            uint8_t zp = nes_read(nes, nes->PC++);
            uint16_t ptr = (uint8_t)(zp + nes->X); // stay on 0x00-0xFF
            uint16_t addr = nes->ram[ptr] | (nes->ram[(ptr+1)&0xFF] << 8);
            if (addr == 0x0000) {
                printf("⚠️ RLA pointer invalid, skipping\n");
                break;
            }


            uint8_t value = nes_read(nes, addr);

            bool old_c = nes->P & FLAG_C;
            nes->P &= ~FLAG_C;
            if (value & 0x80) nes->P |= FLAG_C;

            value = (value << 1) | (old_c ? 1 : 0);
            nes->ram[addr] = value;

            nes->A &= value;
            update_NZ_flags(nes, nes->A);

            printf("RLA: ZP=0x%02X, addr=0x%04X, val=0x%02X, A=0x%02X\n", zp, addr, value, nes->A);
            break;
        }

        case 0x24: { // BIT zeropage
            printf("BIT zeropage at PC=0x%04X\n", nes->PC - 1);

            uint8_t addr = nes_read(nes, nes->PC++);
            uint8_t operand = nes_read(nes, addr);

            nes->P &= ~FLAG_Z;
            if ((nes->A & operand) == 0)
                nes->P |= FLAG_Z;

            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0x25: { // AND Zeropage
            printf("AND zeropage at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes_read(nes, nes->PC++);
            uint8_t value = nes_read(nes, addr);
            nes->A &= value;
            update_NZ_flags(nes, nes->A);
            break;
        }


        case 0x26: { // ROL zeropage
            printf("ROL zeropage at PC=0x%04X\n", nes->PC - 1);

            uint8_t addr = nes->ram[nes->PC++];
            
            uint8_t val = nes->ram[addr];

            uint8_t old_carry = (nes->P & FLAG_C) ? 1 : 0;

            if (val & 0x80) nes->P |= FLAG_C;
            else nes->P &= ~FLAG_C;

            val = (val << 1) | old_carry;

            nes->ram[addr] = val;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (val == 0) nes->P |= FLAG_Z;
            if (val & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0x29: { // AND immediate
            printf("AND immediate at PC=0x%04X\n", nes->PC - 1);

            uint8_t value = nes_read(nes, nes->PC++);
            nes->A &= value;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->A == 0) nes->P |= FLAG_Z;
            if (nes->A & 0x80) nes->P |= FLAG_N;

            break;
        }


        case 0x2C: { // BIT absolute
            printf("BIT at PC=0x%04X\n", nes->PC - 1);
            
            
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint8_t operand = nes_read(nes, addr);

            if ((nes->A & operand) == 0)
                nes->P |= FLAG_Z;
            else
                nes->P &= ~FLAG_Z;

            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0x2D: { // AND absolute
            printf("AND absolute at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint8_t value = nes_read(nes, addr);

            nes->A &= value;

            nes->P &= ~(FLAG_Z | FLAG_N);
            if (nes->A == 0) nes->P |= FLAG_Z;
            if (nes->A & 0x80) nes->P |= FLAG_N;

            break;
        }


        case 0x30: { // BMI relative
            printf("BMI at PC=0x%04X\n", nes->PC - 1);

            int8_t offset = nes_read(nes, nes->PC++);

            if (nes->P & FLAG_N) {
                nes->PC += offset;
            }

            break;
        }

        case 0x34: { // NOP zeropage,X
            printf("NOP zeropage,X at PC=0x%04X\n", nes->PC - 1);
            nes->PC++;
            break;
        }

        case 0x35: { // AND Zeropage,X
            printf("AND zeropage,X at PC=0x%04X\n", nes->PC - 1);
            uint8_t base = nes_read(nes, nes->PC++);
            uint8_t addr = (base + nes->X) & 0xFF;
            uint8_t value = nes_read(nes, addr);
            nes->A &= value;
            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0x38: { // SED — Set Decimal Flag
            printf("SEC at PC=0x%04X\n", nes->PC - 1);

            nes->P |= FLAG_C;
            break;
        }

        case 0x3E: { // ROL absolute,X
            printf("ROL absolute,X at PC=0x%04X\n", nes->PC - 1);

            uint16_t base = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint16_t addr = base + nes->X;

            uint8_t val = nes->ram[addr];

            uint8_t old_carry = (nes->P & FLAG_C) ? 1 : 0;

            if (val & 0x80) nes->P |= FLAG_C;
            else nes->P &= ~FLAG_C;

            val = (val << 1) | old_carry;

            nes->ram[addr] = val;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (val == 0) nes->P |= FLAG_Z;
            if (val & 0x80) nes->P |= FLAG_N;

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

        case 0x45: { // EOR Zeropage - Exclusive OR with Zeropage
            uint8_t addr = nes_read(nes, nes->PC++);
            uint8_t value = nes_read(nes, addr);
            printf("EOR zeropage at PC=0x%04X, addr=0x%02X, value=0x%02X, A=0x%02X\n",
                nes->PC - 1, addr, value, nes->A);
            nes->A ^= value;
            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0x46: { // LSR $nn
            printf("LSR at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes_read(nes, nes->PC++);
            cpu_lsr(nes, addr, false);
            break;
        }

        case 0x48: { // PHA - Push Accumulator
            printf("PHA at PC=0x%04X\n", nes->PC - 1);

            nes->ram[0x0100 + nes->SP] = nes->A;
            nes->SP--;

            break;
        }


        case 0x49: {
            printf("EOR at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->A = nes->A ^ nes->ram[addr];
            update_NZ_flags(nes, nes->A);
            nes->PC += 2;
            break;
        }

        case 0x4A: { // LSR A
            printf("LSR A at PC=0x%04X\n", nes->PC - 1);

            if (nes->A & 0x01) nes->P |= FLAG_C;
            else nes->P &= ~FLAG_C;

            nes->A >>= 1;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->A == 0) nes->P |= FLAG_Z;
 
            break;
        }


        case 0x4C: { // JMP absolute
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            printf("JMP to 0x%04X at PC=0x%04X\n", addr, nes->PC);
            nes->PC = addr;
            break;
        }


        case 0x60: {
            printf("RTS at PC=0x%04X\n", nes->PC - 1);

            uint8_t pcl = nes->ram[0x0100 + ++nes->SP];  // pop low byte
            uint8_t pch = nes->ram[0x0100 + ++nes->SP];  // pop high byte

            nes->PC = ((uint16_t)pch << 8) | pcl;
            nes->PC++;

            break;
        }

        case 0x63: { // RRA (indirect,X)
            uint8_t zp = nes_read(nes, nes->PC++);
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

        case 0x64:
            printf("NOP at PC=0x%04X\n", nes->PC - 1);
            break;

        case 0x66: { // ROR Zeropage - Rotate Right (with carry)
            uint8_t addr = nes_read(nes, nes->PC++);
            uint8_t value = nes_read(nes, addr);
            printf("ROR zeropage at PC=0x%04X, addr=0x%02X, value=0x%02X, carry=%d\n",
                nes->PC - 1, addr, value, (nes->P & FLAG_C) ? 1 : 0);

            uint8_t old_carry = (nes->P & FLAG_C) ? 0x80 : 0x00;
            if (value & 0x01) {
                nes->P |= FLAG_C;
            } else {
                nes->P &= ~FLAG_C;
            }

            value = (value >> 1) | old_carry;
            nes_write(nes, addr, value);

            update_NZ_flags(nes, value);
            break;
        }

        case 0x68: { // PLA - Pull Accumulator from stack
            printf("PLA at PC=0x%04X, SP=0x%02X\n", nes->PC - 1, nes->SP);
            nes->A = nes->ram[0x0100 + ++nes->SP];
            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0x69: { // ADC immediate
            printf("ADC at PC=0x%04X\n", nes->PC - 1);

            uint8_t value = nes_read(nes, nes->PC); 
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

            uint8_t operand = nes_read(nes, nes->PC++);
            nes->A &= operand;

            bool old_c = nes->P & FLAG_C;
            nes->P &= ~(FLAG_C | FLAG_V | FLAG_Z | FLAG_N); // clear flags
            nes->A = (nes->A >> 1) | (old_c ? 0x80 : 0x00);

            // Flag Update
            if (nes->A == 0) nes->P |= FLAG_Z;          // Z
            if (nes->A & 0x80) nes->P |= FLAG_N;        // N
            if (nes->A & 0x40) nes->P |= FLAG_V;        // V = bit 6 après rotation
            if (nes->A & 0x01) nes->P |= FLAG_C;        // C = bit 0 après rotation

            break;
        }

        case 0x70: { // BVS relative
            int8_t offset = nes_read(nes, nes->PC++);
            if (nes->P & FLAG_V) { // flag V = 1 ?
                nes->PC += offset; // do the jump
            }
            break;
        }

        case 0x7E: { // ROR absolute,X
            printf("ROR absolute,X at PC=0x%04X\n", nes->PC - 1);

            uint16_t base = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint16_t addr = base + nes->X;

            uint8_t val = nes->ram[addr];

            uint8_t old_carry = (nes->P & FLAG_C) ? 0x80 : 0;

            if (val & 0x01) nes->P |= FLAG_C;
            else nes->P &= ~FLAG_C;

            val = (val >> 1) | old_carry;

            nes->ram[addr] = val;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (val == 0) nes->P |= FLAG_Z;
            if (val & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0x78:
            printf("SEI at PC=0x%04X\n", nes->PC - 1);
            nes->P |= FLAG_I;
            break;

        case 0x80: { // NOP immediate
            uint8_t operand = nes->prg_memory[nes->PC];
            nes->PC += 1;
            printf("NOP #$%02X at PC=0x%04X\n", operand, nes->PC - 2);
            break;
        }


        case 0x84: { // STY zeropage
            printf("STY zeropage at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes_read(nes, nes->PC++);
            nes_write(nes, addr, nes->Y);
            break;
        }

        case 0x85: { // STA zeropage
            printf("STA zeropage at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes_read(nes, nes->PC++);
            nes_write(nes, addr, nes->A);
            break;
        }

        case 0x86: { // STX Zeropage - Store X register in memory (Zeropage)
            uint8_t addr = nes_read(nes, nes->PC++);
            printf("STX zeropage at PC=0x%04X, addr=0x%02X, X=0x%02X\n",
                nes->PC - 1, addr, nes->X);
            nes_write(nes, addr, nes->X);
            break;
        }

        case 0x88: { // DEY
            printf("DEY at PC=0x%04X\n", nes->PC - 1);
            nes->Y -= 1;
            
            nes->P &= ~(FLAG_Z | FLAG_N);
            if (nes->Y == 0) nes->P |= FLAG_Z;
            if (nes->Y & 0x80) nes->P |= FLAG_N;
            break;
        }

        case 0x8A: { // TXA - Transfer X to Accumulator
            printf("TXA at PC=0x%04X\n", nes->PC - 1);
            nes->A = nes->X;
            update_NZ_flags(nes, nes->A);
            break;
        }


        case 0x8C: { // STY absolute
            printf("STY absolute at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;
            nes_write(nes, addr, nes->Y);
            break;
        }

        case 0x8D: {  // STA absolute
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            printf("STA: Writing A=0x%02X to addr=0x%04X\n", nes->A, addr);
            nes_write(nes, addr, nes->A);
            nes->PC += 2;
            break;
        }

        case 0x8E: {
            printf("STX at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes_write(nes, addr, nes->X);
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

        case 0x91: { // STA (Indirect),Y
            printf("STA (Indirect),Y at PC=0x%04X\n", nes->PC - 1);
            
            uint8_t zp = nes_read(nes, nes->PC++);
            uint16_t base = nes_read(nes, zp) | (nes_read(nes, (uint8_t)(zp + 1)) << 8);
            uint16_t addr = base + nes->Y;
            
            nes_write(nes, addr, nes->A);
            break;
        }

        case 0x94: { // STY zeropage,X 
            printf("STY zeropage,X at PC=0x%04X\n", nes->PC - 1);
            uint8_t base = nes_read(nes, nes->PC++);
            uint8_t addr = (base + nes->X) & 0xFF;
            nes_write(nes, addr, nes->Y);
            break;
        }

        case 0x95: { // STA zeropage,X
            printf("STA zeropage,X at PC=0x%04X\n", nes->PC - 1);
            uint8_t zp_addr = nes_read(nes, nes->PC++);
            uint8_t addr = (zp_addr + nes->X) & 0xFF;
            nes_write(nes, addr, nes->A);
            break;
        }

        case 0x98: { // TYA - Transfer Y to Accumulator
            printf("TYA at PC=0x%04X\n", nes->PC - 1);
            nes->A = nes->Y;
            update_NZ_flags(nes, nes->A);
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
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes_write(nes, addr, nes->A);
            nes->PC += 2;
            break;
        }

        case 0x9E: { // SHX absolute,Y
            printf("SHX absolute,Y at PC=0x%04X\n", nes->PC - 1);

            uint16_t base_addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint16_t addr = base_addr + nes->Y;

            uint8_t high_byte_plus_1 = ((addr >> 8) + 1) & 0xFF; // +1 comme le doc
            uint8_t value = nes->X & high_byte_plus_1;

            nes->ram[addr] = value;

            break;
        }

        case 0xA0: {
            printf("LDY #$%02X at PC=0x%04X\n", nes->ram[nes->PC], nes->PC);
            nes->Y = nes_read(nes, nes->PC++);
            
            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->Y == 0) nes->P |= FLAG_Z;
            if (nes->Y & 0x80) nes->P |= FLAG_N;
            break;
        }

        case 0xA1: { // LDA (Indirect,X)
            printf("LDA (Indirect,X) at PC=0x%04X\n", nes->PC - 1);

            uint8_t zp_addr = nes_read(nes, nes->PC);
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
            nes->X = nes_read(nes, nes->PC);
            nes->PC += 1;
            
            nes->P &= ~(FLAG_Z | FLAG_N); // Clear Z & N
            if (nes->X == 0) nes->P |= FLAG_Z; // Set Z if 0
            if (nes->X & FLAG_N) nes->P |= FLAG_N; // Set N if bit 7
            break;
        }

        case 0xA3: {
            printf("LAX (indirect,X) at PC=0x%04X\n", nes->PC - 1);
            uint8_t zp = nes_read(nes, nes->PC++);
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

            uint8_t addr = nes_read(nes, nes->PC++);
            nes->A = nes->ram[addr];

            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0xA6: { // LDX Zeropage
            printf("LDX zeropage at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes_read(nes, nes->PC++);
            nes->X = nes_read(nes, addr);
            update_NZ_flags(nes, nes->X);
            break;
        }

        case 0xA7: {
            printf("LAX zeropage at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes_read(nes, nes->PC++);
            uint8_t value = nes->ram[addr];
            nes->A = value;
            nes->X = value;
            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0xA8: { // TAY - Transfer Accumulator to Y
            printf("TAY at PC=0x%04X, A=0x%02X\n", nes->PC - 1, nes->A);
            nes->Y = nes->A;
            update_NZ_flags(nes, nes->Y);
            break;
        }
        
        case 0xA9: { // LDA immediate
            printf("LDA #$%02X at PC=0x%04X\n", nes_read(nes, nes->PC), nes->PC - 1);
            nes->A = nes_read(nes, nes->PC);
            nes->PC++;

            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0xAA: { // TAX - Transfer Accumulator to X
            printf("TAX at PC=0x%04X\n", nes->PC - 1);
            nes->X = nes->A;
            update_NZ_flags(nes, nes->X);
            break;
        }

        case 0xAD: {
            printf("LDA at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->A = nes_read(nes, addr);
            nes->PC += 2;
            break;
        }

        case 0xAF: {
            printf("LAX absolute at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;
            uint8_t value = nes_read(nes, addr);
            nes->A = value;
            nes->X = value;
            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0xB0: {
            printf("BCS at PC=0x%04X\n", nes->PC - 1);
            int8_t offset = nes_read(nes, nes->PC);
            nes->PC++;
            if (nes->P & FLAG_C) { // if flag C = 1
                nes->PC += offset; // jump
            }
            break;
        }

        case 0xB3: {
            printf("LAX (indirect),Y at PC=0x%04X\n", nes->PC - 1);
            uint8_t zp = nes_read(nes, nes->PC++);
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

            uint8_t base = nes_read(nes, nes->PC++);
            uint8_t addr = (base + nes->X) & 0xFF;
            nes->A = nes->ram[addr];

            update_NZ_flags(nes, nes->A);

            break;
        }


        case 0xB7: {
            printf("LAX zeropage,Y at PC=0x%04X\n", nes->PC - 1);
            uint8_t base = nes_read(nes, nes->PC++);
            uint8_t addr = (base + nes->Y) & 0xFF;
            uint8_t value = nes->ram[addr];
            nes->A = value;
            nes->X = value;

            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0xB8: { // CLV
            printf("CLV at PC=0x%04X\n", nes->PC - 1);
            nes->P &= ~0x40; // Clear Overflow flag (V = bit 6)
            break;
        }

        case 0xB9: { // LDA absolute,Y
            printf("LDA absolute,Y at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;
            
            uint16_t effective_addr = addr + nes->Y;

            nes->A = nes_read(nes, effective_addr);

            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0xBC: { // absolute, X
            printf("LDY absolute,X at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint16_t effective_addr = addr + nes->X;
            nes->Y = nes_read(nes, effective_addr);

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->Y == 0) nes->P |= FLAG_Z;
            if (nes->Y & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0xBD: { // LDA absolute,X
            printf("LDA absolute,X at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;
            
            uint16_t effective_addr = addr + nes->X;

            nes->A = nes_read(nes, effective_addr);

            update_NZ_flags(nes, nes->A);

            break;
        }

        case 0xBF: {
            printf("LAX absolute,Y at PC=0x%04X\n", nes->PC - 1);
            uint16_t base = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;
            uint16_t addr = base + nes->Y;
            uint8_t value = nes_read(nes, addr);
            nes->A = value;
            nes->X = value;

            update_NZ_flags(nes, nes->A);
            break;
        }

        case 0xC0: { // CPY immediate
            printf("CPY immediate at PC=0x%04X\n", nes->PC - 1);

            uint8_t value = nes_read(nes, nes->PC++);
            uint8_t result = nes->Y - value;

            nes->P &= ~(FLAG_Z | FLAG_N | FLAG_C);
            if (nes->Y >= value) nes->P |= FLAG_C;
            if (nes->Y == value) nes->P |= FLAG_Z;
            if (result & 0x80)   nes->P |= FLAG_N;

            break;
        }


        case 0xC3: {
            printf("DCP at PC=0x%04X\n", nes->PC - 1);
            uint8_t zp = nes_read(nes, nes->PC++);
            uint16_t addr = nes->ram[(zp + nes->X) & 0xFF] | (nes->ram[(zp + nes->X + 1) & 0xFF] << 8);
            nes->ram[addr] -= 1;
            uint8_t value = nes->ram[addr];
            uint16_t result = nes->A - value;

            if (result & 0x80) nes->P |= FLAG_N; else nes->P &= ~FLAG_N;
            if ((result & 0xFF) == 0) nes->P |= FLAG_Z; else nes->P &= ~FLAG_Z;
            if (nes->A >= value) nes->P |= FLAG_C; else nes->P &= ~FLAG_C;
            break;
        }

        case 0xC4: { // CPY zeropage
            printf("CPY zeropage at PC=0x%04X\n", nes->PC - 1);

            uint8_t addr = nes->ram[nes->PC++];
            uint8_t value = nes_read(nes, addr);
            uint8_t result = nes->Y - value;

            nes->P &= ~(FLAG_Z | FLAG_N | FLAG_C);
            if (nes->Y >= value) nes->P |= FLAG_C;
            if (nes->Y == value) nes->P |= FLAG_Z;
            if (result & 0x80) nes->P |= FLAG_N;

            break;
        }


        case 0xC6: { // DEC zeropage
            printf("DEC zeropage at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes_read(nes, nes->PC); // 1 bytes
            nes->PC += 1;

            nes->ram[addr] -= 1;

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (nes->ram[addr] == 0) nes->P |= FLAG_Z;
            if (nes->ram[addr] & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0xC7: { // DCP zeropage
            printf("DCP zeropage at PC=0x%04X\n", nes->PC - 1);

            uint8_t addr = nes_read(nes, nes->PC++);
            nes->ram[addr] -= 1;
            uint8_t value = nes->ram[addr];

            uint16_t result = nes->A - value;

            // Flags
            if ((result & 0xFF) == 0) nes->P |= FLAG_Z; else nes->P &= ~FLAG_Z;
            if (nes->A >= value) nes->P |= FLAG_C; else nes->P &= ~FLAG_C;
            if (result & 0x80) nes->P |= FLAG_N; else nes->P &= ~FLAG_N;

            break;
        }

        case 0xC8: { // INY
            printf("INY at PC=0x%04X\n", nes->PC - 1);
            nes->Y += 1;
            update_NZ_flags(nes, nes->Y);
            break;
        }

        case 0xC9: { // CMP immediate
            uint8_t value = nes_read(nes, nes->PC); // get immediat operande
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

        case 0xCA:
            printf("DEX at PC=0x%04X\n", nes->PC - 1);
            nes->X -= 1;
            nes->P &= ~(FLAG_Z | FLAG_N);
            if (nes->X == 0) nes->P |= FLAG_Z;
            if (nes->X & 0x80) nes->P |= FLAG_N;
            nes->PC++;
            break;

        case 0xCB: { // SBX immediate
            printf("SBX #oper at PC=0x%04X\n", nes->PC - 1);

            uint8_t operand = nes_read(nes, nes->PC++);

            uint16_t result = (nes->A & nes->X) - operand;
            nes->X = result & 0xFF;

            nes->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
            if (nes->X == 0) nes->P |= FLAG_Z;
            if (nes->X & 0x80) nes->P |= FLAG_N;
            if ((nes->A & nes->X) >= operand) nes->P |= FLAG_C; // C = A&X >= operand

            break;
        }

        case 0xCC: { // CPY absolute
            printf("CPY absolute at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            uint8_t value = nes_read(nes, addr);
            uint8_t result = nes->Y - value;

            nes->P &= ~(FLAG_Z | FLAG_N | FLAG_C);
            if (nes->Y >= value) nes->P |= FLAG_C;
            if (nes->Y == value) nes->P |= FLAG_Z;
            if (result & 0x80)   nes->P |= FLAG_N;

            break;
        }


        case 0xCE: { // DEC absolute
            printf("DEC absolute at PC=0x%04X\n", nes->PC - 1);

            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;

            nes->ram[addr] -= 1;
            uint8_t value = nes->ram[addr];

            nes->P &= ~(FLAG_N | FLAG_Z);
            if (value == 0) nes->P |= FLAG_Z;
            if (value & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0xE0: {
            printf("CPX #$%02X at PC=0x%04X\n", nes->prg_memory[nes->PC], nes->PC - 1);
            uint8_t value = nes->prg_memory[nes->PC];
            nes->PC += 1;

            uint8_t result = nes->X - value;

            if (nes->X >= value) {
                nes->P |= FLAG_C;
            } else {
                nes->P &= ~FLAG_C;
            }

            update_NZ_flags(nes, result);
            break;
        }


        case 0xE6: {
            printf("INC at PC=0x%04X\n", nes->PC - 1);
            uint8_t addr = nes_read(nes, nes->PC);
            nes->ram[addr] += 1;
            uint8_t value = nes->ram[addr];
            nes->P &= ~(FLAG_Z | FLAG_N);
            if (value == 0) nes->P |= FLAG_Z;
            if (value & FLAG_N) nes->P |= FLAG_N;
            nes->PC++;
            break;
        }

        case 0xE7: { // SBC Zero Page
            uint8_t zp_addr = nes->prg_memory[nes->PC];
            nes->PC += 1;

            uint8_t value = nes_read(nes, zp_addr);
            uint16_t temp = nes->A - value - (1 - ((nes->P & FLAG_C) ? 1 : 0));

            printf("SBC $%02X at PC=0x%04X\n", zp_addr, nes->PC - 2);

            nes->A = temp & 0xFF;

            if (temp <= 0xFF)
                nes->P |= FLAG_C;
            else
                nes->P &= ~FLAG_C;

            update_NZ_flags(nes, nes->A);

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

        case 0xEC: { // CPX absolute
            uint16_t addr = nes->prg_memory[nes->PC] | (nes->prg_memory[nes->PC + 1] << 8);
            nes->PC += 2;

            uint8_t value = nes_read(nes, addr);
            uint8_t result = nes->X - value;

            printf("CPX $%04X at PC=0x%04X\n", addr, nes->PC - 3);

            if (nes->X >= value)
                nes->P |= FLAG_C;
            else
                nes->P &= ~FLAG_C;

            update_NZ_flags(nes, result);
            break;
        }

        
        case 0xEE: {
            printf("INC (absolute) at PC=0x%04X\n", nes->PC - 1);
            uint16_t addr = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            uint8_t value = ++nes->ram[addr];
            nes->P &= ~(FLAG_Z | FLAG_N);
            if (value == 0) nes->P |= FLAG_Z;  // Zero
            if (value & FLAG_N) nes->P |= FLAG_N; // Negative
            nes->PC += 2;
            break;
        }

        case 0xD0: { // BNE — Branch if Not Equal (Z = 0)
            printf("BNE at PC=0x%04X\n", nes->PC - 1);

            int8_t offset = nes_read(nes, nes->PC++);
            
            if (!(nes->P & FLAG_Z)) {
                nes->PC += offset;
            }

            break;
        }

        case 0xD8:
            printf("CLD at PC=0x%04X\n", nes->PC - 1);
            nes->P &= ~FLAG_D;
            break;

        case 0xD9: { // CMP absolute,Y
            printf("CMP absolute,Y at PC=0x%04X\n", nes->PC - 1);

            uint16_t base = nes->ram[nes->PC] | (nes->ram[nes->PC + 1] << 8);
            nes->PC += 2;

            uint16_t addr = base + nes->Y;

            uint8_t value = nes->ram[addr];

            uint8_t result = nes->A - value;

            nes->P &= ~(FLAG_Z | FLAG_N | FLAG_C);
            if (nes->A >= value) nes->P |= FLAG_C;
            if (nes->A == value) nes->P |= FLAG_Z;
            if (result & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0xDC: { // NOP absolute,X (illegal)
            printf("NOP absolute,X at PC=0x%04X\n", nes->PC - 1);
            nes->PC += 2;
            break;
        }

        case 0xDD: { // CMP absolute,X
            printf("CMP absolute,X at PC=0x%04X\n", nes->PC - 1);

            uint16_t base = nes->ram[nes->PC] | (nes->ram[nes->PC + 1] << 8);
            nes->PC += 2;

            uint16_t addr = base + nes->X;

            uint8_t value = nes->ram[addr];

            uint8_t result = nes->A - value;

            nes->P &= ~(FLAG_Z | FLAG_N | FLAG_C);
            if (nes->A >= value) nes->P |= FLAG_C;
            if (nes->A == value) nes->P |= FLAG_Z;
            if (result & 0x80) nes->P |= FLAG_N;

            break;
        }


        case 0xF0: {
            printf("BEQ at PC=0x%04X\n", nes->PC - 1);
            int8_t offset = nes_read(nes, nes->PC);
            nes->PC++;
            if (nes->P & FLAG_Z)  // if flag Z = 1
                nes->PC += offset; // jump
            break;
        }

        case 0xF1: { // SBC ($nn),Y
            printf("SBC ($nn),Y at PC=0x%04X\n", nes->PC - 1);

            uint8_t zp_addr = nes_read(nes, nes->PC++);

            uint16_t base = nes->ram[zp_addr] | (nes->ram[(zp_addr + 1) & 0xFF] << 8);
            uint16_t addr = base + nes->Y;

            uint8_t value = nes_read(nes, addr);

            uint16_t result = nes->A - value - ((nes->P & 0x01) ? 0 : 1);

            if (result < 0x100)
                nes->P |= 0x01;
            else
                nes->P &= ~0x01;

            uint8_t overflow = ((nes->A ^ result) & 0x80) && ((nes->A ^ value) & 0x80);
            if (overflow)
                nes->P |= 0x40;
            else
                nes->P &= ~0x40;

            if ((result & 0xFF) == 0)
                nes->P |= 0x02;
            else
                nes->P &= ~0x02;

            if (result & 0x80)
                nes->P |= 0x80;
            else
                nes->P &= ~0x80;

            nes->A = (uint8_t)result;
            break;
        }

        case 0xF5: { // SBC zeropage,X
            printf("SBC zeropage,X at PC=0x%04X\n", nes->PC - 1);

            uint8_t zp_addr = nes_read(nes, nes->PC++);
            uint8_t addr = (zp_addr + nes->X) & 0xFF;
            uint8_t value = nes->ram[addr];

            uint16_t result = nes->A - value - ((nes->P & 0x01) ? 0 : 1);

            if (result < 0x100)
                nes->P |= 0x01;
            else
                nes->P &= ~0x01;

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

        case 0xF6: {  
            printf("INC zeropage,X at PC=0x%04X\n", nes->PC - 1);
            uint8_t base = nes_read(nes, nes->PC++);
            uint8_t addr = (base + nes->X) & 0xFF;

            nes->ram[addr] += 1;
            uint8_t value = nes->ram[addr];

            nes->P &= ~(FLAG_Z | FLAG_N);
            if (value == 0) nes->P |= FLAG_Z;
            if (value & 0x80) nes->P |= FLAG_N;

            break;
        }

        case 0xF7: { // ISC (Zeropage,X) - Increment then Subtract with Carry
            uint8_t base = nes_read(nes, nes->PC++);
            uint8_t addr = (base + nes->X) & 0xFF;
            nes->ram[addr] += 1;
            uint8_t m = nes->ram[addr];
            uint16_t result = nes->A - m - ((nes->P & FLAG_C) ? 0 : 1);
            nes->P &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
            if (result & 0x80) nes->P |= FLAG_N;
            if ((result & 0xFF) == 0) nes->P |= FLAG_Z;
            if (result < 0x100) nes->P |= FLAG_C;
            if (((nes->A ^ result) & (0xFF ^ m) & 0x80) != 0) nes->P |= FLAG_V;
            nes->A = result & 0xFF;
            break;
        }

        case 0xF8: { // SED — Set Decimal Flag
            printf("SED at PC=0x%04X\n", nes->PC - 1);

            nes->P |= FLAG_D;
            break;
        }

                
        case 0xFC: { // NOP absolute,X (illegal)
            printf("NOP absolute,X at PC=0x%04X\n", nes->PC - 1);
            nes->PC += 2;
            break;
        }

        case 0xFE: {  
            printf("INC absolute,X at PC=0x%04X\n", nes->PC - 1);

            uint16_t base = nes_read(nes, nes->PC) | (nes_read(nes, nes->PC + 1) << 8);
            nes->PC += 2;
            uint16_t addr = base + nes->X;

            nes->ram[addr] += 1;
            uint8_t value = nes->ram[addr];

            nes->P &= ~(FLAG_Z | FLAG_N);
            if (value == 0) nes->P |= FLAG_Z;
            if (value & 0x80) nes->P |= FLAG_N;

            break;
        }


        case 0xFF: {  // ISC / ISB / INS zeropage,X
            printf("ISC (zeropage,X) at PC=0x%04X\n", nes->PC - 1);

            
            uint8_t base = nes_read(nes, nes->PC++);
            uint8_t addr = (base + nes->X) & 0xFF;

            // INC M
            nes->ram[addr] += 1;
            uint8_t m = nes->ram[addr];

            // SBC M (A = A - M - (1 - C))
            uint16_t result = nes->A - m - ((nes->P & FLAG_C) ? 0 : 1);

            // flags
            nes->P &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
            if (result & 0x80) nes->P |= FLAG_N;
            if ((result & 0xFF) == 0) nes->P |= FLAG_Z;
            if (result < 0x100) nes->P |= FLAG_C;
            if (((nes->A ^ result) & (0xFF ^ m) & 0x80) != 0) nes->P |= FLAG_V;

            nes->A = result & 0xFF;

            break;
        }

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
            printf("⚠️ JAM opcode 0x%02X at PC=0x%04X, skipping\n", opcode, nes->PC - 1);
            nes->PC++; // jmp instruc so as not to block
            break;

        
        default:
            printf("❌ Unimplemented opcode: %02X at PC=0x%04X\n", opcode, nes->PC - 1);
            exit(1);

    }

}