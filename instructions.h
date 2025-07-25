void GB_Loop();

struct gbRegisters {
    uint8_t a; uint8_t f;
    uint8_t b; uint8_t c;
    uint8_t d; uint8_t e;
    uint8_t h; uint8_t l;
};

enum gbFlags{
    CARRY = 4,
    HALFCARRY,
    BORROW,
    ZERO
};

uint8_t GB_GetFlag(int flag);
void GB_SetFlag(int flag, int value);
