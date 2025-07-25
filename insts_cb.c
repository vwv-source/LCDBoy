#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "instructions.h"

extern uint16_t pc;
extern uint16_t sp;
extern long c;
extern struct gbRegisters regs;
extern uint8_t memory[0x10000];

void GB_MemoryWrite();

void SWAP_n(uint8_t* reg){
    *reg = ((*reg & 0x0F) << 4u | (*reg & 0xF0) >> 4u);
    pc+=2;
    c+=8;
}

void SRL_n(uint8_t* reg){
    GB_SetFlag(CARRY, *reg & 0x1u);
    *reg = *reg >> 1u;

    GB_SetFlag(ZERO, *reg == 0 ? 1 : 0);
    GB_SetFlag(BORROW,0);
    GB_SetFlag(HALFCARRY,0);

    pc+=2;
    c+=8;
}

void RR_n(uint8_t* reg){
    uint8_t copy = *reg;
    *reg = (GB_GetFlag(CARRY) ? 0x80 : 0) | (*reg >> 1u);

    GB_SetFlag(ZERO, *reg == 0 ? 1 : 0);
    GB_SetFlag(CARRY, copy & 0x1u);

    pc+=2;
    c+=8;
}

void SLA_n(uint8_t* reg){
    GB_SetFlag(CARRY, (*reg & 0x80u) >> 7u);
    *reg = *reg << 1u;

    GB_SetFlag(ZERO, *reg == 0 ? 1 : 0);
    GB_SetFlag(BORROW,0);
    GB_SetFlag(HALFCARRY,0);

    pc+=2;
    c+=8;
}

void BIT(uint8_t* reg, uint8_t bit){
    if(((*reg >> bit) & 0x1u) == 0){
	    GB_SetFlag(ZERO, 1);
    }else{
	    GB_SetFlag(ZERO, 0);
    }

    GB_SetFlag(BORROW, 0);
    GB_SetFlag(HALFCARRY, 1);

    pc+=2;
    c+=8;
}

void RST(uint8_t* reg, uint8_t bit){
    *reg &= ~(0x1u << (bit));
    pc+=2;
    c+=8;
}

void SET(uint8_t* reg, uint8_t bit){
    *reg |= (0x1u << (bit));
    pc+=2;
    c+=8;
}

void handleCB(uint16_t opcode){
    uint16_t HL = (regs.h << 8u) | regs.l;

    switch(opcode & 0x00FFu){
        case 0x37:
            SWAP_n(&regs.a); break;
        case 0x33:
            SWAP_n(&regs.e); break;
        case 0x31:
            SWAP_n(&regs.c); break;
        case 0x30:
            SWAP_n(&regs.b); break;

        //RES 0 (HL)
        case 0x86:
            GB_MemoryWrite(HL, memory[HL] & ~(1u<<(0)));
            //memory[HL] &= ~(1u<<(0));
            pc+=2; c+=16; break;

        //RL C
        case 0x11:
            GB_SetFlag(CARRY, (regs.c & 0x80) >> 7u);
            GB_SetFlag(HALFCARRY, 0);
            GB_SetFlag(BORROW, 0);
            regs.c = (regs.c << 1u) | (GB_GetFlag(CARRY) ? 1 : 0);
            GB_SetFlag(ZERO, regs.c ? 0 : 1);
            pc+=2; c+=8; break;

        case 0x12:
            GB_SetFlag(CARRY, (regs.d & 0x80) >> 7u);
            GB_SetFlag(HALFCARRY, 0);
            GB_SetFlag(BORROW, 0);
            regs.d = (regs.d << 1u) | (GB_GetFlag(CARRY) ? 1 : 0);
            GB_SetFlag(ZERO, regs.d ? 0 : 1);
            pc+=2; c+=8; break;

        //SRL n
        case 0x3F:
            SRL_n(&regs.a); break;
        case 0x38:
            SRL_n(&regs.b); break;
        case 0x39:
            SRL_n(&regs.c); break;
        case 0x3A:
            SRL_n(&regs.d); break;
        case 0x3B:
            SRL_n(&regs.e); break;
        case 0x3C:
            SRL_n(&regs.h); break;
        case 0x3D:
            SRL_n(&regs.l); break;
        case 0x3E:
            SRL_n(&memory[HL]); break;

        //RR n
        case 0x1F:
            RR_n(&regs.a); break;
        case 0x18:
            RR_n(&regs.b); break;
        case 0x19:
            RR_n(&regs.c); break;
        case 0x1A:
            RR_n(&regs.d); break;
        case 0x1B:
            RR_n(&regs.e); break;
        case 0x1C:
            RR_n(&regs.h); break;
        case 0x1D:
            RR_n(&regs.l); break;
        case 0x1E:
            RR_n(&regs.l); break;


        //SLA n
        case 0x27:
            SLA_n(&regs.a); break;
        case 0x20:
            SLA_n(&regs.b); break;
        case 0x21:
            SLA_n(&regs.c); break;
        case 0x22:
            SLA_n(&regs.d); break;
        case 0x23:
            SLA_n(&regs.e); break;
        case 0x24:
            SLA_n(&regs.h); break;
        case 0x25:
            SLA_n(&regs.l); break;
        case 0x26:
            SLA_n(&memory[HL]); break;

        //RES 0,A
        case 0x87:
            regs.a &= ~(1u<<(0));
            c+=8; pc+=2;
            break;

        default:
            //why am I repeating all of this crap
            //I should just make a function instead
            uint8_t* regpointers[8] = {&regs.b, &regs.c, &regs.d, &regs.e, &regs.h, &regs.l, &memory[HL], &regs.a};
            if( (opcode & 0x00FFu) > 0x3Fu && (opcode & 0x00FFu) < 0x80u){
                for(uint8_t i = 0; i < 8; i++){
                    for(uint8_t j = 0; j < 8; j++){
                        if((opcode & 0x00FFu) == (0x40u + i*8+j)){
                            BIT(regpointers[j], i);
                        }
                    }
                }
            }else if( (opcode & 0x00FFu) > 0x7Fu && (opcode & 0x00FFu) < 0xC0u){
                for(uint8_t i = 0; i < 8; i++){
                    for(uint8_t j = 0; j < 8; j++){
                        if((opcode & 0x00FFu) == (0x80u + i*8+j)){
                            RST(regpointers[j], i);
                        }
                    }
                }
            }else if( (opcode & 0x00FFu) > 0xBFu){
                for(uint8_t i = 0; i < 8; i++){
                    for(uint8_t j = 0; j < 8; j++){
                        if((opcode & 0x00FFu) == (0xC0u + i*8+j)){
                            SET(regpointers[j], i);
                        }
                    }
                }
            }else{
                printf("UNIMPLEMENTED CB INSTRUCTION!!\n");
                printf("INST: 0x%x PC: 0x%x\n", opcode, pc);
                exit(0);
            }
        }
}
