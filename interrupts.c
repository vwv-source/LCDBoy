#include <stdint.h>

#include "instructions.h"
#include "interrupts.h"

extern int ime;
extern int halted;
extern uint16_t pc;
extern uint16_t sp;
extern uint8_t * memory;
extern uint16_t opcode;

static void jumpReset(uint16_t location,int interrupt){
    ime = 0;
    memory[IF] &= ~(1u<<(interrupt));
    
    sp-=2;
    memory[sp+1] = (pc & 0xFF00u) >> 8u;
    memory[sp] = (pc & 0x00FFu);

    pc = location;
}

void handleInterrupts(){
    if(halted && memory[IF] & 0x1F){
        halted = 0;
    }

    if(ime){
        if(memory[IF] & 0x1u){ //VBLANK
            jumpReset(0x0040u, INTERRUPT_VBLANK);
        }else if(memory[IF] & 0x2u){ //STAT
            jumpReset(0x0048u, INTERRUPT_STAT);
        }else if(memory[IF] & 0x4u){ //TIMER
            jumpReset(0x0050u, INTERRUPT_TIMER_OVERFLOW);
        }
    }
}

int interruptEnabled(int interrupt){
    return (ime && ((memory[IE] >> interrupt) & 0x1));
}