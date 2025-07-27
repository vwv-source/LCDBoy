#include <stdint.h>

int GB_runInstruction();
void runCPUTicks(int tcycles);

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

enum cmnRegisters{
    LCDC = 0xFF40,
    LCDC_Y = 0xFF44,
    IE = 0xFFFF,
    DIV = 0xFF04,
    LYC = 0xFF45,
    STAT = 0xFF41,
    SCX = 0xFF43,
    SCY = 0xFF42,
    IF = 0xFF0F,
    TAC = 0xFF07,
    TIMA = 0xFF05
};

uint8_t GB_GetFlag(int flag);
void GB_SetFlag(int flag, int value);
