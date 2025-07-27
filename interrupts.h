void handleInterrupts();
int interruptEnabled(int interrupt);

enum interrupts{
    INTERRUPT_VBLANK,
    INTERRUPT_STAT,
    INTERRUPT_TIMER_OVERFLOW,
    INTERRUPT_SERIAL,
    INTERRUPT_TRANSITION
};