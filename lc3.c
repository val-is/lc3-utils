#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "lc3.h"

uint16_t
spl_pc(LC3 *l, uint16_t a, uint16_t b) {
    uint16_t r = 0;
    for (uint16_t i = a; i < b; i++)
        r += l->mem[l->reg[R_PC]-1] & (1 << i);
    return r >> a;
}

uint16_t
sign_extend(uint16_t a, uint16_t signif) {
    uint16_t sign = a & (1 << (signif-1));
    a |= (sign ? 0xffff : 0) << signif;
    return a;
}

void
update_flags(LC3 *l, uint16_t r) {
    if (l->reg[r] == 0)
        l->reg[R_COND] = COND_ZER;
    else if (l->reg[r] & 0x8000)
        l->reg[R_COND] = COND_NEG;
    else
        l->reg[R_COND] = COND_POS;
}

void
op_br(LC3 *l) {
    l->reg[R_PC] += 
        (spl_pc(l, 11, 12) && (l->reg[R_COND] & COND_NEG)) || 
        (spl_pc(l, 10, 11) && (l->reg[R_COND] & COND_ZER)) ||
        (spl_pc(l,  9, 10) && (l->reg[R_COND] & COND_POS)) ? sign_extend(spl_pc(l, 0, 9), 9) : 0;
}

void
op_add(LC3 *l) {
    l->reg[spl_pc(l, 9, 12)] = l->reg[spl_pc(l, 6, 9)] + (spl_pc(l, 5, 6) ?
        sign_extend(spl_pc(l, 0, 5), 5) : l->reg[spl_pc(l, 0, 3)]);
    update_flags(l, spl_pc(l, 9, 12));
}

void
op_ld(LC3 *l) {
    l->reg[spl_pc(l, 9, 12)] = l->mem[l->reg[R_PC] + spl_pc(l, 0, 9)];
    update_flags(l, spl_pc(l, 9, 12));
}

void
op_st(LC3 *l) {
    l->mem[l->reg[R_PC] + spl_pc(l, 0, 9)] = l->reg[spl_pc(l, 9, 12)];
}

void
op_jsr(LC3 *l) {
    if (spl_pc(l, 11, 12))
        l->reg[R_PC] += sign_extend(spl_pc(l, 0, 11), 11);
    else
        l->reg[R_PC] = l->reg[spl_pc(l, 6, 9)];
}

void
op_and(LC3 *l) {
    l->reg[spl_pc(l, 9, 12)] = l->reg[spl_pc(l, 6, 9)] & (spl_pc(l, 5, 6) ?
            sign_extend(spl_pc(l, 0, 5), 5) : l->reg[spl_pc(l, 0, 3)]);
    update_flags(l, spl_pc(l, 9, 12));
}

void
op_ldr(LC3 *l) {
    l->reg[spl_pc(l, 9, 12)] = l->mem[l->reg[spl_pc(l, 6, 9)] + sign_extend(spl_pc(l, 0, 6), 6)];
    update_flags(l, spl_pc(l, 9, 12));
}

void
op_str(LC3 *l) {
    l->mem[l->reg[spl_pc(l, 6, 9)] + sign_extend(spl_pc(l, 0, 6), 6)] = l->reg[spl_pc(l, 9, 12)];
}

void
op_rti(LC3 *l) {
}

void
op_not(LC3 *l) {
    l->reg[spl_pc(l, 9, 12)] = ~l->reg[spl_pc(l, 6, 9)];
    update_flags(l, spl_pc(l, 9, 12));
}

void
op_ldi(LC3 *l) {
    l->reg[spl_pc(l, 9, 12)] = l->mem[l->reg[R_PC] + sign_extend(spl_pc(l, 0, 9), 9)];
    update_flags(l, spl_pc(l, 9, 12));
}

void
op_sti(LC3 *l) {
    l->mem[l->reg[R_PC] + sign_extend(spl_pc(l, 0, 9), 9)] = l->reg[spl_pc(l, 9, 12)];
}

void
op_jmp(LC3 *l) {
    l->reg[R_PC] = l->reg[spl_pc(l, 6, 9)];
}

void
op_res(LC3 *l) {
}

void
op_lea(LC3 *l) {
    l->reg[spl_pc(l, 9, 12)] = l->reg[R_PC] + sign_extend(spl_pc(l, 0, 9), 9);
    update_flags(l, spl_pc(l, 9, 12));
}

void
t_getc(LC3 *l) {
    l->reg[R0] = (uint16_t) getchar();
}

void
t_out(LC3 *l) {
    putc((char) (l->reg[R0] & 0x00FF), stdout);
    fflush(stdout);
}

void
t_puts(LC3 *l) {
    uint16_t *c = l->mem + l->reg[R0];
    while (*c) {
        putc((char) *c, stdout);
        c++;
    }
    fflush(stdout);
}

void
t_in(LC3 *l) {
    printf(">> ");
    char c = getchar();
    putc(c, stdout);
    fflush(stdout);
    l->reg[R0] = (uint16_t) c;
}

void
t_putsp(LC3 *l) {
    uint16_t *c = l->mem + l->reg[R0];
    while (*c) {
        putc((char) (*c & 0x00FF), stdout);
        putc((char) ((*c & 0xFF00) >> 8), stdout);
    }
    fflush(stdout);
}

void
t_halt(LC3 *l) {
    /* puts("\nhalted."); */
    l->running = 0;
}

void
op_trap(LC3 *l) {
    uint8_t trap = spl_pc(l, 0, 8);
    switch (trap) {
    case T_GETC:
        t_getc(l);
        return;
    case T_OUT:
        t_out(l);
        return;
    case T_PUTS:
        t_puts(l);
        return;
    case T_IN:
        t_in(l);
        return;
    case T_PUTSP:
        t_putsp(l);
        return;
    case T_HALT:
        t_halt(l);
        return;
    case T_DUMP:
        dump_regs(l);
        return;
    case T_BREAK:
        getchar();
        return;
    }
}

void (*ops[])(LC3 *l) = {
    op_br,  op_add, op_ld,  op_st,
    op_jsr, op_and, op_ldr, op_str,
    op_rti, op_not, op_ldi, op_sti,
    op_jmp, op_res, op_lea, op_trap
};

void
step_lc3(LC3 *l) {
    uint16_t op = l->mem[l->reg[R_PC]];
    l->reg[R_PC] += 1;
    ops[(op & 0xf000) >> 12](l);
}

void
run_lc3(LC3 *l) {
    while (l->running) {
        step_lc3(l);
    }
}

void
dump_regs(LC3 *l) {
    uint16_t op = l->mem[l->reg[R_PC]];
    printf("op: %x\n", (op & 0xf000) >> 12);
    printf("code: %x\n", op);
    for (short r=0; r<R_COUNT; r++) {
        printf("r%d: %x\n", r, l->reg[r]);
    }
}
