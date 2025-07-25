#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "display.h"
#include "insts_cb.h"

typedef struct {
    uint8_t a; uint8_t f;
    uint8_t b; uint8_t c;
    uint8_t d; uint8_t e;
    uint8_t h; uint8_t l;
} gbRegisters;

enum gbFlags{
    CARRY = 4,
    HALFCARRY,
    BORROW,
    ZERO
};

extern uint16_t pc;
extern uint16_t sp;
extern uint8_t memory[0x10000];
extern uint16_t opcode;

extern uint8_t P14buffer;
extern uint8_t P15buffer;

uint32_t opcode3;
long c = 0;
int ime = 0;
gbRegisters regs;

void GB_ReadRangeIntoMemory();

uint8_t GB_GetFlag(int flag){
    return ((regs.f >> flag) & 0x1);
}

void GB_SetFlag(int flag, int value){
    if(value == 0){
        regs.f &= ~(1u<<(flag));
    }else{
        regs.f |= (1u<<(flag));
    }
}

void GB_CheckHalfCarry(uint8_t int1, uint8_t int2){
    //god fuck this
    if( ((int1 & 0x0Fu) + (int2 & 0x0Fu) ) > 0xFu ){
        GB_SetFlag(HALFCARRY, 1);
    }else{
        GB_SetFlag(HALFCARRY, 0);
    }
}

void GB_CheckHalfBorrow(uint8_t int1, uint8_t int2){
    if((int1 & 0x0F) < (int2 & 0x0F)){
        GB_SetFlag(HALFCARRY, 1);
    }else{
        GB_SetFlag(HALFCARRY, 0);
    }
}

//0 is 16/8 (default)
//1 is 4/32
int MBC1_MemoryModel = 0;
void GB_MemoryWrite(uint16_t address, uint8_t value){

	//most basic mbc1 config
	if(memory[0x147] == 0x1u){
		switch(address){

		//memory model selection
		case 0x6000u ... 0x8000u:
			MBC1_MemoryModel = value & 0x1;
			return;

		//4000-7FFF bank selection
		case 0x2000u ... 0x4000u:
			GB_ReadRangeIntoMemory(value & 0x1Fu);
			//printf("[MBC1 BANK SELECT] Address: 0x%x, Value: 0x%x, Bank: 0x%x, ROMS: 0x%x, ROME: 0x%x\n", address, value, value & 0x1Fu, 0x4000u * (value & 0x1Fu), 0x4000u * (value & 0x1Fu) + 0x4000u - 0x1u);
			return;

		}
	}


	if(address == 0xFF46u){
		uint16_t bigvalue = value << 8u;
		for(uint8_t i = 0; i < 160; i++){
			memory[0xFE00u + i] = memory[bigvalue + i];
		}
	}

	if(address == 0xFF00u){
		if(value >> 4u == 0x1){
			memory[0xFF00u] = P15buffer;
		}
		else{
			memory[0xFF00u] = P14buffer;
		}
	}

	//Add the rest of the restricted areas dumbass
	if(address > 0x8000u && address != 0xFF00u){
		memory[address] = value;
	}

}

//LD NN N
void LD_nn(uint8_t* reg){
    uint8_t nn = (opcode & 0x00FFu);
    *reg = nn;
    c+=8;
    pc+=2;
}

//LD R1 R2
void LD_r1(uint8_t* reg1, uint8_t* reg2){
    *reg1 = *reg2;
    c+=4;
    pc+=1;
}

//LD R1 R2 but with HL
void LDHL_r1(uint8_t* reg, int side){
    uint16_t HL = (regs.h << 8u) | regs.l;
    
    if(side){
        *reg = memory[HL];
    }else{
        GB_MemoryWrite(HL, *reg);
    }
    
    c+=8;
    pc+=1;
}

//LD R1 R2 but not limited to HL
void LD16_r1(uint8_t* reg, uint16_t merged, int side){
    if(side){
        *reg = memory[merged];
    }else{
        GB_MemoryWrite(merged, *reg);
    }
    
    c+=8;
    pc+=1;
}

void LD_n_nn(uint8_t *reg, uint8_t *reg2, uint16_t nn){
    *reg2 = (nn & 0xFF00u) >> 8u;
    *reg = nn & 0x00FFu;
    c+=12;
    pc+=3;
};

void LDHL_SP_n(uint8_t *reg, uint8_t *reg2, uint8_t n){
    GB_CheckHalfCarry(sp & 0x00FFu, n);
    uint32_t temp = sp + (int8_t)n;
    *reg = (temp & 0xFF00u) >> 8u;
    *reg2 = (temp & 0x00FFu);

    GB_SetFlag(ZERO, 0);
    GB_SetFlag(BORROW, 0);

    GB_SetFlag(CARRY, (sp & 0x00FFu) + n > 0xFFu ? 1 : 0);

    c+=12;
    pc+=2;
}

void PUSH_POP(uint8_t *reg, uint8_t *reg2, int type){
    if(type){
        GB_MemoryWrite(--sp, *reg);
        GB_MemoryWrite(--sp, *reg2);
        c+=16;
    }else{
        *reg2 = memory[sp++];
        *reg = memory[sp++];
        c+=12;
    }
    pc+=1;
}

void ADD_A(uint8_t* reg2){
    uint16_t temp; 
    temp = regs.a + *reg2;
    GB_CheckHalfCarry(regs.a, *reg2);
    regs.a = (uint8_t)temp;

    GB_SetFlag(ZERO, regs.a ? 0 : 1);
    GB_SetFlag(BORROW, 0);
    GB_SetFlag(CARRY, (temp > 0xFFu) ? 1 : 0);
}

void ADC_A(uint8_t* reg2){
    uint16_t temp;

    if( ((regs.a & 0x0Fu) + (*reg2 & 0x0Fu) + GB_GetFlag(CARRY))> 0xFu ){
        GB_SetFlag(HALFCARRY, 1);
    }else{
        GB_SetFlag(HALFCARRY, 0);
    }

    temp = regs.a + *reg2 + GB_GetFlag(CARRY);
    regs.a = temp & 0x00FFu;

    GB_SetFlag(ZERO, regs.a ? 0 : 1);
    GB_SetFlag(BORROW, 0);
    GB_SetFlag(CARRY, temp > 0xFF ? 1 : 0);
}

void LDD_I(int type){
    uint16_t HL = (regs.h << 8u) | regs.l;
    regs.a = memory[HL];

    type ? HL++ : HL--;

    regs.h = (HL & 0xFF00u) >> 8u;
    regs.l = HL & 0x00FFu;
    c+=8;
    pc+=1;
}

void LDHLD_I(int type){
    uint16_t HL = (regs.h << 8u) | regs.l;
    GB_MemoryWrite(HL, regs.a);

    type ? HL++ : HL--;

    regs.h = (HL & 0xFF00u) >> 8u;
    regs.l = HL & 0x00FFu;
    c+=8;
    pc+=1;
}

void SUB_A(uint8_t* reg2){
    uint16_t temp;
    temp = regs.a - *reg2;

    GB_SetFlag(CARRY, *reg2 > regs.a ? 1 : 0);
    GB_CheckHalfBorrow(regs.a, *reg2);

    regs.a = (uint8_t)temp;

    GB_SetFlag(ZERO, regs.a ? 0 : 1);
    GB_SetFlag(BORROW, 1);
}

void SBC_A(uint8_t* reg2){
    uint16_t temp;
    uint8_t regcopy = regs.a;
    temp = regs.a - (*reg2 + GB_GetFlag(CARRY));


    if(((regs.a & 0x0Fu) - (*reg2 & 0x0Fu) - GB_GetFlag(CARRY) ) & 0x10u){
        GB_SetFlag(HALFCARRY, 1);
    }else{
        GB_SetFlag(HALFCARRY, 0);
    }

    regs.a = (uint8_t)temp;

    GB_SetFlag(ZERO, regs.a ? 0 : 1);
    GB_SetFlag(BORROW, 1);
    GB_SetFlag(CARRY, *reg2+GB_GetFlag(CARRY) > regcopy ? 1 : 0);
}

void AND_n(uint8_t* reg2){
    regs.a &= *reg2;
    
    GB_SetFlag(ZERO, regs.a ? 0 : 1);

    GB_SetFlag(BORROW, 0);
    GB_SetFlag(HALFCARRY, 1);
    GB_SetFlag(CARRY, 0);
}

void OR_n(uint8_t* reg2){
    regs.a |= *reg2;
    
    regs.f = 0x0;
    GB_SetFlag(ZERO, regs.a ? 0 : 1);
}

void XOR_n(uint8_t* reg2){
    regs.a ^= *reg2;
    
    regs.f = 0x0;
    GB_SetFlag(ZERO, regs.a ? 0 : 1);
}

void CP_n(uint8_t* reg2){
    uint16_t temp = regs.a - *reg2;

    GB_SetFlag(ZERO, temp ? 0 : 1);
    GB_SetFlag(BORROW, 1);
    GB_CheckHalfBorrow(regs.a, *reg2);
    GB_SetFlag(CARRY, regs.a < *reg2 ? 1 : 0);
}

void INC_n(uint8_t* reg){
    uint8_t regcopy = *reg;

    GB_CheckHalfCarry(*reg, 0x1);
    *reg += 1;

    GB_SetFlag(ZERO, *reg ? 0 : 1);
    GB_SetFlag(BORROW, 0);

    pc+=1;
    c+=4;
}

void DEC_n(uint8_t* reg){
    uint8_t regcopy = *reg;
    *reg -= 1;

    GB_SetFlag(ZERO, *reg ? 0 : 1);
    GB_SetFlag(BORROW, 1);
    GB_CheckHalfBorrow(regcopy, 0x1u);

    pc+=1;
    c+=4;
}

//TO-CHECK
void ADD_HL_n(uint8_t* reg, uint8_t* reg2) {
    uint16_t hl = (regs.h << 8u) | regs.l;
    uint16_t n = (*reg << 8u) | *reg2;
    uint32_t result = hl + n;

    regs.h = (result & 0xFF00u) >> 8u;
    regs.l = result & 0x00FFu;

    GB_SetFlag(BORROW, 0);
    GB_SetFlag(HALFCARRY, ((hl & 0x0FFF) + (n & 0x0FFF) > 0x0FFF) ? 1 : 0);
    GB_SetFlag(CARRY, result > 0xFFFFu ? 1 : 0);

    pc += 1;
    c += 8;
}

void INC_nn(uint8_t* reg, uint8_t* reg2){
    uint16_t combined = (*reg << 8u) | *reg2;
    combined++;

    *reg = (combined & 0xFF00u) >> 8u;
    *reg2 = (combined & 0x00FFu);

    pc+=1;
    c+=8;
}

void DEC_nn(uint8_t* reg, uint8_t* reg2){
    uint16_t combined = (*reg << 8u) | *reg2;
    combined--;

    *reg = (combined & 0xFF00u) >> 8u;
    *reg2 = (combined & 0x00FFu);

    pc+=1;
    c+=8;
}


void Generic_CALL(uint16_t address){
    if(sp > 0x8000){
        pc+=1;
        GB_MemoryWrite(--sp, (pc & 0xFF00u) >> 8u);
        GB_MemoryWrite(--sp, (pc & 0x00FFu));
    } 
    pc = address;
    c+=32;
}

void Generic_RET(){
    pc = ((memory[sp+1] << 8u) | memory[sp]);
    sp+=2;
    c+=8;
}

int onlyonce = 0;
int breakreached = 0;

int pcycles, dcycles, tcycles = 0;

void GB_Loop(){
    regs.f &= 0xF0;

    if(c >= 456*pcycles){
    	if(breakreached)
	        memory[0xFF44] = 0x90;
        else
			DisplayCycle();
			
        pcycles++;
    }

    if(c >= 256*dcycles){
        memory[0xFF04] += 1;
        dcycles++;
    }
    
    //cursed timer interrupt
    if(ime && (memory[0xFF0Fu] >> 2u) & 0x1u){
	    ime = 0;
        memory[0xFF0Fu] = 0x0;
	    sp--;
	    memory[sp] = (pc & 0xFF00u) >> 8u;
	    sp--;
	    memory[sp] = (pc & 0x00FFu);
	    pc = 0x0030u;
    }

    //??? what the fuck was I making
    //if(((memory[0xFF07] >> 2u) & 0x1) == 1){
    //    switch(memory[0xFF07] & 0x3u){

    //    }
    //}


    //alternative to using the boot rom
	//just manually initializing the registers instead
    if(!onlyonce){
	    onlyonce = 1;
	    regs.a = 0x01;
	    regs.f = 0xB0;
	    regs.c = 0x13;
	    regs.e = 0xd8;
	    regs.h = 0x01;
	    regs.l = 0x4d;
	    sp = 0xFFFE;
    }

    opcode = (memory[pc] << 8u) | memory[pc+1];
    opcode3 = (memory[pc] << 16u) | memory[pc+1] << 8u | memory[pc+2];
    
    uint16_t AF = (regs.a << 8u) | regs.f;
    uint16_t DE = (regs.d << 8u) | regs.e;
    uint16_t BC = (regs.b << 8u) | regs.c;
    uint16_t HL = (regs.h << 8u) | regs.l;

    uint8_t n = (opcode & 0x00FFu);
    uint16_t nn = (opcode3 & 0x00FFFFu);
    uint16_t LSnn = ((opcode3 & 0x0000FFu) << 8u) | ((opcode3 & 0x00FF00u) >> 8u);
    uint16_t calc_temp = 0;

    if(breakreached == 1){
		//printf("INST: 0x%x\nPC: 0x%x SP: 0x%x\n", (opcode & 0xFF00u) >> 8u, pc, sp);
		//printf("Flags: Z:%u N:%u H:%u C:%u\n", GB_GetFlag(ZERO), GB_GetFlag(BORROW), GB_GetFlag(HALFCARRY), GB_GetFlag(CARRY), regs.a);
		//printf("Registers: A:%u B:%u C:%u D:%u E:%u F:%u H:%u L:%u\n", 
		//        regs.a, regs.b, regs.c, regs.d, regs.e, regs.f, regs.h, regs.l);
		//printf("Combined Registers: AF:%u BC:%u DE:0x%x HL:0x%x\n", AF, BC, DE, HL);
		//printf("Misc: LCDC:0x%x LY:0x%x IE:0x%x IME:%i FF07(TAC):0x%x\n\n", memory[0xFF40u], memory[0xFF44u], memory[0xFFFFu], ime, memory[0xFF07u]);
	    printf("A:%02x F:%02x B:%02x C:%02x D:%02x E:%02x H:%02x L:%02x SP:%04x PC:%04x PCMEM:%02x,%02x,%02x,%02x\n",regs.a, regs.f,regs.b,regs.c,regs.d,regs.e,regs.h,regs.l,sp,pc,memory[pc], memory[pc+1], memory[pc+2], memory[pc+3]);
    }

	//todo
	//try to be more smart instead of using so many switch statements
	//there's a very obvious pattern here like the 0xCB instruction set
    switch((opcode & 0xFF00u) >> 8u){
        //NOP
        case 0x00:
            c+=4; pc+=1;
            break;

        //LD NN, N
        case 0x06:
            LD_nn(&regs.b); break;
        case 0x0E:
            LD_nn(&regs.c); break;
        case 0x16:
            LD_nn(&regs.d); break;
        case 0x1E:
            LD_nn(&regs.e); break;
        case 0x26:
            LD_nn(&regs.h); break;
        case 0x2E:
            LD_nn(&regs.l); break;

        //LD R1 R2
        case 0x7F:
            LD_r1(&regs.a, &regs.a); break;
        case 0x78:
            LD_r1(&regs.a, &regs.b); break;
        case 0x79:
            LD_r1(&regs.a, &regs.c); break;
        case 0x7A:
            LD_r1(&regs.a, &regs.d); break;
        case 0x7B:
            LD_r1(&regs.a, &regs.e); break;
        case 0x7C:
            LD_r1(&regs.a, &regs.h); break;
        case 0x7D:
            LD_r1(&regs.a, &regs.l); break;
        case 0x7E:
            LDHL_r1(&regs.a, 1); break;

        case 0x40:
            LD_r1(&regs.b, &regs.b); break;
        case 0x41:
            LD_r1(&regs.b, &regs.c); break;
        case 0x42:
            LD_r1(&regs.b, &regs.d); break;
        case 0x43:
            LD_r1(&regs.b, &regs.e); break;
        case 0x44:
            LD_r1(&regs.b, &regs.h); break;
        case 0x45:
            LD_r1(&regs.b, &regs.l); break;
        case 0x46:
            LDHL_r1(&regs.b, 1); break;        

        case 0x48:
            LD_r1(&regs.c, &regs.b); break;
        case 0x49:
            LD_r1(&regs.c, &regs.c); break;
        case 0x4A:
            LD_r1(&regs.c, &regs.d); break;
        case 0x4B:
            LD_r1(&regs.c, &regs.e); break;
        case 0x4C:
            LD_r1(&regs.c, &regs.h); break;
        case 0x4D:
            LD_r1(&regs.c, &regs.l); break;
        case 0x4E:
            LDHL_r1(&regs.c, 1); break;    

        case 0x50:
            LD_r1(&regs.d, &regs.b); break;
        case 0x51:
            LD_r1(&regs.d, &regs.c); break;
        case 0x52:
            LD_r1(&regs.d, &regs.d); break;
        case 0x53:
            LD_r1(&regs.d, &regs.e); break;
        case 0x54:
            LD_r1(&regs.d, &regs.h); break;
        case 0x55:
            LD_r1(&regs.d, &regs.l); break;
        case 0x56:
            LDHL_r1(&regs.d, 1); break;  
        
        case 0x58:
            LD_r1(&regs.e, &regs.b); break;
        case 0x59:
            LD_r1(&regs.e, &regs.c); break;
        case 0x5A:
            LD_r1(&regs.e, &regs.d); break;
        case 0x5B:
            LD_r1(&regs.e, &regs.e); break;
        case 0x5C:
            LD_r1(&regs.e, &regs.h); break;
        case 0x5D:
            LD_r1(&regs.e, &regs.l); break;
        case 0x5E:
            LDHL_r1(&regs.e, 1); break; 

        case 0x60:
            LD_r1(&regs.h, &regs.b); break;
        case 0x61:
            LD_r1(&regs.h, &regs.c); break;
        case 0x62:
            LD_r1(&regs.h, &regs.d); break;
        case 0x63:
            LD_r1(&regs.h, &regs.e); break;
        case 0x64:
            LD_r1(&regs.h, &regs.h); break;
        case 0x65:
            LD_r1(&regs.h, &regs.l); break;
        case 0x66:
            LDHL_r1(&regs.h, 1); break; 

        case 0x68:
            LD_r1(&regs.l, &regs.b); break;
        case 0x69:
            LD_r1(&regs.l, &regs.c); break;
        case 0x6A:
            LD_r1(&regs.l, &regs.d); break;
        case 0x6B:
            LD_r1(&regs.l, &regs.e); break;
        case 0x6C:
            LD_r1(&regs.l, &regs.h); break;
        case 0x6D:
            LD_r1(&regs.l, &regs.l); break;
        case 0x6E:
            LDHL_r1(&regs.l, 1); break; 

        case 0x70:
            LDHL_r1(&regs.b, 0); break; 
        case 0x71:
            LDHL_r1(&regs.c, 0); break; 
        case 0x72:
            LDHL_r1(&regs.d, 0); break; 
        case 0x73:
            LDHL_r1(&regs.e, 0); break; 
        case 0x74:
            LDHL_r1(&regs.h, 0); break; 
        case 0x75:
            LDHL_r1(&regs.l, 0); break; 
        case 0x36:
            if(HL > 0x8000u){
                GB_MemoryWrite(HL, opcode & 0x00FFu);
                //memory[HL] = (opcode & 0x00FFu);
            }
            c+=12; pc+=2;
            break;

        //LD A,n
        case 0x0A:
            LD16_r1(&regs.a, BC, 1); break;
        case 0x1A:
            LD16_r1(&regs.a, DE, 1); break;
        case 0xFA:
            regs.a = memory[LSnn];
            c+=16; pc+=3;
            break;
        case 0x3E:
            regs.a = n;
            c+=8; pc+=2;
            break;

        //LD n,A
        case 0x47:
            LD_r1(&regs.b, &regs.a); break;
        case 0x4F:
            LD_r1(&regs.c, &regs.a); break;
        case 0x57:
            LD_r1(&regs.d, &regs.a); break;
        case 0x5F:
            LD_r1(&regs.e, &regs.a); break;
        case 0x67:
            LD_r1(&regs.h, &regs.a); break;
        case 0x6F:
            LD_r1(&regs.l, &regs.a); break;
        case 0x02:
            LD16_r1(&regs.a, BC, 0); break;
        case 0x12:
            LD16_r1(&regs.a, DE, 0); break;
        case 0x77:
            LD16_r1(&regs.a, HL, 0); break;
        case 0xEA:
            GB_MemoryWrite(LSnn, regs.a);
            pc+=3; c+=16;
            break;

        //LD A,(C)
        case 0xF2:
            regs.a = memory[0xFF00u + regs.c];
            c+=8; pc+=1;
	    break;

        //LD (C),A
        case 0xE2:
            GB_MemoryWrite(0xFF00u + regs.c, regs.a);
            c+=8; pc+=1;
	    break;

        //LDD A,(HL)
        case 0x3A:
            LDD_I(0);
            break;
        
        //LDD (HL), A
        case 0x32:
            LDHLD_I(0);
            break;

        //LDI A,(HL)
        case 0x2A:
            LDD_I(1);
            break;

        //LDI (HL), A
        case 0x22:
            LDHLD_I(1);
            break;

        //LDH (n), A
        case 0xE0:
            GB_MemoryWrite(0xFF00u + n, regs.a);
            c+=12; pc+=2;
            break;

        //LDH A,(n)
        case 0xF0:
            regs.a = memory[0xFF00u + n];
            c+=12; pc+=2;
            break;

        //LD n,nn
        case 0x01:
            LD_n_nn(&regs.b, &regs.c, nn); break;
        case 0x11:
            LD_n_nn(&regs.d, &regs.e, nn); break;
        case 0x21:
            LD_n_nn(&regs.h, &regs.l, nn); break;
        case 0x31:
            sp = LSnn;
            c+=12;
            pc+=3;
            break;

        //LD SP,HL
        case 0xF9:
            sp = HL;
            c+=8;
            pc+=1;
            break;

        //LDHDL SP,n
        case 0xF8:
            LDHL_SP_n(&regs.h, &regs.l, n);
            break;
        
        //LD (nn),SP
        case 0x08:
            GB_MemoryWrite(LSnn, sp & 0x00FFu);
            GB_MemoryWrite(LSnn+1, (sp & 0xFF00u) >> 8u);
            c+=20;
            pc+=3;
            break;
        
        //PUSH nn
        case 0xF5:
            PUSH_POP(&regs.a, &regs.f, 1); break;
        case 0xC5:
            PUSH_POP(&regs.b, &regs.c, 1); break;
        case 0xD5:
            PUSH_POP(&regs.d, &regs.e, 1); break;
        case 0xE5:
            PUSH_POP(&regs.h, &regs.l, 1); break;

        //POP nn
        case 0xF1:
            PUSH_POP(&regs.a, &regs.f, 0);
            break;
        case 0xC1:
            PUSH_POP(&regs.b, &regs.c, 0); break;
        case 0xD1:
            PUSH_POP(&regs.d, &regs.e, 0); break;
        case 0xE1:
            PUSH_POP(&regs.h, &regs.l, 0); break;

        //Beginning of 8-bit Arithmetic Logic Unit
        //ADD A,n
        case 0x87:
            ADD_A(&regs.a); pc+=1; c+=4; break;
        case 0x80:
            ADD_A(&regs.b); pc+=1; c+=4; break;
        case 0x81:
            ADD_A(&regs.c); pc+=1; c+=4; break;
        case 0x82:
            ADD_A(&regs.d); pc+=1; c+=4; break;
        case 0x83:
            ADD_A(&regs.e); pc+=1; c+=4; break;
        case 0x84:
            ADD_A(&regs.h); pc+=1; c+=4; break;
        case 0x85:
            ADD_A(&regs.l); pc+=1; c+=4; break;
        case 0x86:
            ADD_A(&memory[HL]); pc+=1; c+=8; break;
        case 0xC6:
            ADD_A(&n); c+=8; pc+=2; break;

        //ADC A,n
        case 0x8F:
            ADC_A(&regs.a); pc+=1; c+=4; break;
        case 0x88:
            ADC_A(&regs.b); pc+=1; c+=4; break;
        case 0x89:
            ADC_A(&regs.c); pc+=1; c+=4; break;
        case 0x8A:
            ADC_A(&regs.d); pc+=1; c+=4; break;
        case 0x8B:
            ADC_A(&regs.e); pc+=1; c+=4; break;
        case 0x8C:
            ADC_A(&regs.h); pc+=1; c+=4; break;
        case 0x8D:
            ADC_A(&regs.l); pc+=1; c+=4; break;
        case 0x8E:
            ADC_A(&memory[HL]); pc+=1; c+=8; break;
        case 0xCE:
            ADC_A(&n); c+=8; pc+=2; break;
 
        //SUB n
        case 0x97:
            SUB_A(&regs.a); pc+=1; c+=4; break;
        case 0x90:
            SUB_A(&regs.b); pc+=1; c+=4; break;
        case 0x91:
            SUB_A(&regs.c); pc+=1; c+=4; break;
        case 0x92:
            SUB_A(&regs.d); pc+=1; c+=4; break;
        case 0x93:
            SUB_A(&regs.e); pc+=1; c+=4; break;
        case 0x94:
            SUB_A(&regs.h); pc+=1; c+=4; break;
        case 0x95:
            SUB_A(&regs.l); pc+=1; c+=4; break;
        case 0x96:
            SUB_A(&memory[HL]); pc+=1; c+=8; break;
        case 0xD6:
            SUB_A(&n); c+=8; pc+=2; break;
            
        //SBC A,n
        case 0x9F:
            SBC_A(&regs.a); pc+=1; c+=4; break;
        case 0x98:
            SBC_A(&regs.b); pc+=1; c+=4; break;
        case 0x99:
            SBC_A(&regs.c); pc+=1; c+=4; break;
        case 0x9A:
            SBC_A(&regs.d); pc+=1; c+=4; break;
        case 0x9B:
            SBC_A(&regs.e); pc+=1; c+=4; break;
        case 0x9C:
            SBC_A(&regs.h); pc+=1; c+=4; break;
        case 0x9D:
            SBC_A(&regs.l); pc+=1; c+=4; break;
        case 0x9E:
            SBC_A(&memory[HL]); pc+=1; c+=8; break;
        case 0xDE:
            SBC_A(&n); c+=8; pc+=2; break;

        //AND n
        case 0xA7:
            AND_n(&regs.a); pc+=1; c+=4; break;
        case 0xA0:
            AND_n(&regs.b); pc+=1; c+=4; break;
        case 0xA1:
            AND_n(&regs.c); pc+=1; c+=4; break;
        case 0xA2:
            AND_n(&regs.d); pc+=1; c+=4; break;
        case 0xA3:
            AND_n(&regs.e); pc+=1; c+=4; break;
        case 0xA4:
            AND_n(&regs.h); pc+=1; c+=4; break;
        case 0xA5:
            AND_n(&regs.l); pc+=1; c+=4; break;
        case 0xA6:
            AND_n(&memory[HL]); pc+=1; c+=8; break;
        case 0xE6:
            AND_n(&n); c+=8; pc+=2; break;

        //OR n
        case 0xB7:
            OR_n(&regs.a); pc+=1; c+=4; break;
        case 0xB0:
            OR_n(&regs.b); pc+=1; c+=4; break;
        case 0xB1:
            OR_n(&regs.c); pc+=1; c+=4; break;
        case 0xB2:
            OR_n(&regs.d); pc+=1; c+=4; break;
        case 0xB3:
            OR_n(&regs.e); pc+=1; c+=4; break;
        case 0xB4:
            OR_n(&regs.h); pc+=1; c+=4; break;
        case 0xB5:
            OR_n(&regs.l); pc+=1; c+=4; break;
        case 0xB6:
            OR_n(&memory[HL]); pc+=1; c+=8; break;
        case 0xF6:
            OR_n(&n); c+=8; pc+=2; break;
        
        //XOR n
        case 0xAF:
            XOR_n(&regs.a); pc+=1; c+=4; break;
        case 0xA8:
            XOR_n(&regs.b); pc+=1; c+=4; break;
        case 0xA9:
            XOR_n(&regs.c); pc+=1; c+=4; break;
        case 0xAA:
            XOR_n(&regs.d); pc+=1; c+=4; break;
        case 0xAB:
            XOR_n(&regs.e); pc+=1; c+=4; break;
        case 0xAC:
            XOR_n(&regs.h); pc+=1; c+=4; break;
        case 0xAD:
            XOR_n(&regs.l); pc+=1; c+=4; break;
        case 0xAE:
            XOR_n(&memory[HL]); pc+=1; c+=8; break;
        case 0xEE:
            XOR_n(&n); c+=8; pc+=2; break;

        //CP n
        case 0xBF:
            CP_n(&regs.a); pc+=1; c+=4; break;
        case 0xB8:
            CP_n(&regs.b); pc+=1; c+=4; break;
        case 0xB9:
            CP_n(&regs.c); pc+=1; c+=4; break;
        case 0xBA:
            CP_n(&regs.d); pc+=1; c+=4; break;
        case 0xBB:
            CP_n(&regs.e); pc+=1; c+=4; break;
        case 0xBC:
            CP_n(&regs.h); pc+=1; c+=4; break;
        case 0xBD:
            CP_n(&regs.l); pc+=1; c+=4; break;
        case 0xBE:
            CP_n(&memory[HL]); pc+=1; c+=8; break;
        case 0xFE:
            CP_n(&n); c+=8; pc+=2; break;

        //INC n
        case 0x3C:
            INC_n(&regs.a); break;
        case 0x04:
            INC_n(&regs.b); break;
        case 0x0C:
            INC_n(&regs.c); break;
        case 0x14:
            INC_n(&regs.d); break;
        case 0x1C:
            INC_n(&regs.e); break;
        case 0x24:
            INC_n(&regs.h); break;
        case 0x2C:
            INC_n(&regs.l); break;
        case 0x34:
            INC_n(&memory[HL]); c+=8; break;
        
        //DEC n
        case 0x3D:
            DEC_n(&regs.a); break;
        case 0x05:
            DEC_n(&regs.b); break;
        case 0x0D:
            DEC_n(&regs.c); break;
        case 0x15:
            DEC_n(&regs.d); break;
        case 0x1D:
            DEC_n(&regs.e); break;
        case 0x25:
            DEC_n(&regs.h); break;
        case 0x2D:
            DEC_n(&regs.l); break;
        case 0x35:
            DEC_n(&memory[HL]); c+=8; break;

        //End of 8bit ALU

        //Beginning of 16-bit Arithmetic
        //INC nn
        case 0x03:
            INC_nn(&regs.b, &regs.c); break;
        case 0x13:
            INC_nn(&regs.d, &regs.e); break;
        case 0x23:
            INC_nn(&regs.h, &regs.l); break;
        case 0x33:
            sp++; pc+=1; c+=8; break;

		//DEC nn
        case 0x0B:
            DEC_nn(&regs.b, &regs.c); break;
        case 0x1B:
            DEC_nn(&regs.d, &regs.e); break;
        case 0x2B:
            DEC_nn(&regs.h, &regs.l); break;
        case 0x3B:
            sp--; pc+=1; c+=8; break;

        //CALL NZ
        case 0xC4:
            if(!GB_GetFlag(ZERO)){
                pc+=2;
				c+=4;
				Generic_CALL(LSnn);
            }else{
                pc+=3;
                c+=12;
            }
            break;

		//CALL Z
        case 0xCC:
            if(GB_GetFlag(ZERO)){
                pc+=2;
                c+=4;
                Generic_CALL(LSnn);
            }else{
                pc+=3;
                c+=12;
            }
            break;

        case 0xD4:
            if(!GB_GetFlag(CARRY)){
                pc+=2;
				c+=4;
				Generic_CALL(LSnn);
            }else{
                pc+=3;
                c+=12;
            }
            break;

        case 0xDC:
            if(GB_GetFlag(CARRY)){
                pc+=2;
				c+=4;
				Generic_CALL(LSnn);
            }else{
                pc+=3;
                c+=12;
            }
            break;
        //CALL nn
        case 0xCD:
			pc+=2;
			c+=4;
			Generic_CALL(LSnn);
            break;

		//RET?
		case 0xC9:
			Generic_RET();
			break;

   	   	//DI (disable interrupts)
		case 0xF3:
			ime = 0; pc+=1; c+=4; break;

		//EI (enable interrupts)
		case 0xFB:
			ime = 1; pc+=1; c+=4; break;

	    //RST n
	    case 0xC7:
	        Generic_CALL(0x0000u); break;
	    case 0xCF:
	        Generic_CALL(0x0008u); break;
	    case 0xD7:
	        Generic_CALL(0x0010u); break;
	    case 0xDF:
	        Generic_CALL(0x0018u); break;
	    case 0xE7:
	        Generic_CALL(0x0020u); break;
	    case 0xEF:
	        Generic_CALL(0x0028u); break;
	    case 0xF7:
	        Generic_CALL(0x0030u); break;
	    case 0xFF:
	        Generic_CALL(0x0038u); break;

	    //ADD HL n
	    case 0x09:
	        ADD_HL_n(&regs.b, &regs.c); break;
	    case 0x19:
	        ADD_HL_n(&regs.d, &regs.e); break;
	    case 0x29:
	        ADD_HL_n(&regs.h, &regs.l); break;
	    //TO-CHECK
	    case 0x39:
			uint8_t sp_high = (sp & 0xFF00u) >> 8u;
			uint8_t sp_low = sp & 0x00FFu;
			ADD_HL_n(&sp_high, &sp_low);
	        break;

    	case 0xE8:
			GB_CheckHalfCarry(sp & 0x00FFu, n);
			uint16_t result = (sp & 0x00FFu) + n;
			sp = sp + (int8_t)(opcode & 0x00FFu);
			
			GB_SetFlag(ZERO, 0);
			GB_SetFlag(BORROW, 0);
			GB_SetFlag(CARRY, result > 0xFFu ? 1 : 0);
			pc+=2; c+=16;
          	break;
	    
	    //JP cc nn
	    case 0xC2:
			if(GB_GetFlag(ZERO) == 0){
				pc = LSnn;
			}else{
				pc+=3;
			}
			c+=12;
	        break;
	    case 0xCA:
			if(GB_GetFlag(ZERO) == 1){
				pc = LSnn;
			}else{
				pc+=3;
			}
			c+=12;
	        break;
	    case 0xD2:
			if(GB_GetFlag(CARRY) == 0){
				pc = LSnn;
			}else{
				pc+=3;
			}
			c+=12;
	        break;
	    case 0xDA:
			if(GB_GetFlag(CARRY) == 1){
				pc = LSnn;
			}else{
				pc+=3;
			}
			c+=12;
	        break;

	    //RET cc
	    case 0xC0:
			GB_GetFlag(ZERO) == 0 ? Generic_RET() : pc++;
	        break;
	    case 0xC8:
			GB_GetFlag(ZERO) == 1 ? Generic_RET() : pc++;
	        break;
	    case 0xD0:
			GB_GetFlag(CARRY) == 0 ? Generic_RET() : pc++;
	        break;
	    case 0xD8:
			GB_GetFlag(CARRY) == 1 ? Generic_RET() : pc++;
	        break;

	    //JP HL
	    case 0xE9:
			pc = HL;
			c+=4;
        break;

		//JR n
		case 0x18:
			pc += (int8_t)(opcode & 0x00FFu)+2;
			c+=8;
			break;

      	//JR NZ
		case 0x20:
			if(GB_GetFlag(ZERO) == 0){
				//+2 since we're accounting for the JR NZ bytes
				//..I think
				pc += (int8_t)(opcode & 0x00FFu)+2;
			}else{
				pc+=2;
			}
			c+=8;
			break;

		//JR Z
		case 0x28:
			if(GB_GetFlag(ZERO) == 1){
				pc += (int8_t)(opcode & 0x00FFu)+2;
			}else{
				pc+=2;
			}
			c+=8;
			break;

		//JR NC
		case 0x30:
			if(GB_GetFlag(CARRY) == 0){
				pc += (int8_t)(opcode & 0x00FFu)+2;
			}else{
				pc+=2;
			}
			c+=8;
			break;

		//JR C
		case 0x38:
			if(GB_GetFlag(CARRY) == 1){
				pc += (int8_t)(opcode & 0x00FFu)+2;
			}else{
				pc+=2;
			}
			c+=8;
			break;

	    //CPL
	    case 0x2F:
	        regs.a = ~regs.a;
	        GB_SetFlag(BORROW, 1);
	        GB_SetFlag(HALFCARRY, 1);
	        c+=4; pc+=1; break;

		//JP nn
		case 0xC3:
			c+=12;
			pc = ((opcode3 & 0x0000FFu) << 8u) | ((opcode3 & 0x00FF00u) >> 8u);
			break;
	
	    //RLA
	    case 0x17:
	        uint8_t oldBit7 = (regs.a & 0x80u) >> 7u;
	        GB_SetFlag(HALFCARRY, 0);
	        GB_SetFlag(BORROW, 0);

	        regs.a = (regs.a << 1u) | GB_GetFlag(CARRY);

	        GB_SetFlag(ZERO, regs.a ? 0 : 1);
	        GB_SetFlag(CARRY, oldBit7);

	        pc+=1; c+=4; break;

      	//RLCA
	    case 0x07:
	        uint8_t oldBit72 = (regs.a & 0x80u) >> 7u;
	        GB_SetFlag(HALFCARRY, 0);
	        GB_SetFlag(BORROW, 0);

	        regs.a = (regs.a << 1u) | oldBit72;

	        GB_SetFlag(ZERO, regs.a ? 0 : 1);
	        GB_SetFlag(CARRY, oldBit72);

	        pc+=1; c+=4; break;

	    //RRA
	    case 0x1F:
	        uint8_t oldBit0 = (regs.a & 0x1u);
	        regs.a = (GB_GetFlag(CARRY) ? 0x80 : 0) | (regs.a >> 1u);

	        GB_SetFlag(ZERO, 0);
	        GB_SetFlag(BORROW, 0);
	        GB_SetFlag(HALFCARRY, 0);
	        GB_SetFlag(CARRY, oldBit0);

	        pc+=1; c+=8; break;


	    //RETI
	    case 0xD9:
	        Generic_RET();
	        ime=1;
	        c+=8; break;

	    //HALT
	    case 0x76:
	        pc+=1;
	        break;

		//SCF
		case 0x37:
			GB_SetFlag(CARRY, 1);
			GB_SetFlag(BORROW, 0);
			GB_SetFlag(HALFCARRY, 0);
			pc+=1; c+=4;
			break;

		//CCF
		case 0x3F:
			GB_SetFlag(CARRY, GB_GetFlag(CARRY) ? 0 : 1);
			GB_SetFlag(BORROW, 0);
			GB_SetFlag(HALFCARRY, 0);
			pc+=1; c+=4;
			break;

	    //TO-CHECK
	    //DAA
	    case 0x27:
	        if (!GB_GetFlag(CARRY)) {  
                if (GB_GetFlag(CARRY) || regs.a > 0x99) { regs.a += 0x60; GB_SetFlag(CARRY, 1); }
                if (GB_GetFlag(HALFCARRY) || (regs.a & 0x0f) > 0x09) { regs.a += 0x6; }
	        } else {  	     
                if (GB_GetFlag(CARRY)) { regs.a -= 0x60; }
	            if (GB_GetFlag(HALFCARRY)) { regs.a -= 0x6; }
	        }
	        GB_SetFlag(ZERO, regs.a ? 1 : 0); // the usual z flag
	        GB_SetFlag(HALFCARRY, 0); // h flag is always cleared
	        pc+=1;
	        c+=8;
	        break;

		case 0x10:
			break;

	    case 0xCB:
            handleCB(opcode);
            break;
        
        default:
            printf("UNIMPLEMENTED INSTRUCTION!!!!!\n");
            printf("INST: 0x%x PC: 0x%x\n", (opcode & 0xFF00u) >> 8u, pc);
	        getchar();
            exit(0);
    }
}
