#include <stdio.h>
#include <stdint.h>

enum {
    R0 = 0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    R_PC,
    R_COND,
    R_COUNT
};

enum {
    COND_POS = 1 << 0,
    COND_ZER = 1 << 1,
    COND_NEG = 1 << 2
};

enum {
    T_GETC  = 0x20,
    T_OUT   = 0x21,
    T_PUTS  = 0x22,
    T_IN    = 0x23,
    T_PUTSP = 0x24,
    T_HALT  = 0x25
};

typedef struct LC3 {
    uint16_t mem[65536];
    uint16_t reg[R_COUNT];
    uint8_t  running;  
} LC3;

uint16_t sign_extend(uint16_t a, uint16_t signif);
void step_lc3(LC3 *l);
void run_lc3(LC3 *l);
void dump_regs(LC3 *l);
