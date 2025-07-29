#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "instructions.h"

extern uint16_t pc;
extern uint16_t sp;
extern long c;
extern gbRegisters regs;
extern uint8_t * memory;

#define BREAK_IF(cond) if (cond) return;

void GB_MemoryWrite();

void SWAP_n(uint8_t* reg){
    *reg = ((*reg & 0x0F) << 4u | (*reg & 0xF0) >> 4u);

    GB_SetFlag(CARRY, 0);
    GB_SetFlag(ZERO, *reg == 0 ? 1 : 0);
    GB_SetFlag(BORROW, 0);
    GB_SetFlag(HALFCARRY, 0);

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
    GB_SetFlag(BORROW,0);
    GB_SetFlag(HALFCARRY,0);

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

void SRA_n(uint8_t* reg){
    uint8_t oldBit7 = (*reg & 0x80u) >> 7u;
    GB_SetFlag(CARRY, (*reg & 0x1u));
    *reg = (*reg >> 0x1u) | (oldBit7 << 0x7u) ;

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

void RLC(uint8_t* reg){
    uint8_t oldBit7 = (*reg & 0x80u) >> 7u;
    GB_SetFlag(CARRY, oldBit7);
    *reg = (*reg << 0x1u) | oldBit7;

    GB_SetFlag(CARRY, oldBit7);
    GB_SetFlag(ZERO, *reg == 0 ? 1 : 0);
    GB_SetFlag(BORROW, 0);
    GB_SetFlag(HALFCARRY, 0);
    pc+=2;
    c+=8;
}

void RRC(uint8_t* reg){
    uint8_t oldBit1 = (*reg & 0x1u);
    GB_SetFlag(CARRY, oldBit1);
    *reg = (*reg >> 0x1) | (oldBit1 << 7u);

    GB_SetFlag(CARRY, oldBit1);
    GB_SetFlag(ZERO, *reg == 0 ? 1 : 0);
    GB_SetFlag(BORROW, 0);
    GB_SetFlag(HALFCARRY, 0);

    pc+=2;
    c+=8;
}


void RL(uint8_t* reg){
    uint8_t oldBit7 = (*reg & 0x80u) >> 7u;
    *reg = (*reg << 0x1) | GB_GetFlag(CARRY);

    GB_SetFlag(CARRY, oldBit7);
    GB_SetFlag(ZERO, *reg == 0 ? 1 : 0);
    GB_SetFlag(BORROW, 0);
    GB_SetFlag(HALFCARRY, 0);

    pc+=2;
    c+=8;
}

int CB_SimpleLoop(void (*regFunc)(uint8_t*),uint16_t lower, uint16_t upper, uint16_t opcode){
    uint16_t HL = (regs.h << 8u) | regs.l;
    uint8_t* regpointers[8] = {&regs.b, &regs.c, &regs.d, &regs.e, &regs.h, &regs.l, &memory[HL], &regs.a};

    if(!lower && !(opcode & 0x00FFu)){
        regFunc(regpointers[0]);
        return 1;
    }

    if((opcode & 0x00FFu) > lower && (opcode & 0x00FFu) < upper){
        for(uint8_t i = 0; i < 8; i++){
            if((opcode & 0x00FFu) == ((lower ? lower+1 : lower) + i)){
                regFunc(regpointers[i]);
            }
        }
        return 1;
    }   
    return 0;
}

void handleCB(uint16_t opcode){
    uint16_t HL = (regs.h << 8u) | regs.l;
    uint8_t* regpointers[8] = {&regs.b, &regs.c, &regs.d, &regs.e, &regs.h, &regs.l, &memory[HL], &regs.a};

    //probably shouldn't have overcomplicated this :)
    //atleast it's more readable now^^
    BREAK_IF(CB_SimpleLoop(RLC, 0x0u, 0x08u, opcode));
    BREAK_IF(CB_SimpleLoop(RRC, 0x07u, 0x10u, opcode));
    
    BREAK_IF(CB_SimpleLoop(RL, 0x0Fu, 0x18u, opcode));
    BREAK_IF(CB_SimpleLoop(RR_n, 0x17u, 0x20u, opcode));

    BREAK_IF(CB_SimpleLoop(SRA_n, 0x27u, 0x30u, opcode));
    BREAK_IF(CB_SimpleLoop(SLA_n, 0x1Fu, 0x28u, opcode));

    BREAK_IF(CB_SimpleLoop(SWAP_n, 0x2Fu, 0x38u, opcode));

    BREAK_IF(CB_SimpleLoop(SRL_n, 0x37u, 0x40u, opcode));

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
        printf("Unimplemented CB instruction!\n");
        printf("INST: 0x%x PC: 0x%x\n", opcode, pc);
        exit(0);
    }
}
