#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define line_buffer_len 180
#define max_symbol_len 50
#define max_ops_len 100

const char *keywords[] = {
    "LD", "LDI", "LDR", "LEA",
    "ST", "STI", "STR",
    "ADD", "AND", "NOT",
    "BR", "BRn", "BRz", "BRp", "BRnz", "BRzp", "BRnp", "BRnzp",
    "JMP", "JSR", "JSRR", "RET", "RTI",
    "GETC", "OUT", "PUTS", "IN", "PUTSP", "HALT",
    ".ORIG", ".FILL", ".BLKW", ".STRINGZ", ".END",
    "DUMP", "BREAK"
};

const char *regs[] = {"R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7"};

enum {
    P_UNUSED,
    P_REG,
    P_LAB,
    P_LIT
};

struct Line {
    char symbol[max_symbol_len];
    char keyword[10];
    int keyword_index;
    char ops[100];
};

struct Instruction {
    uint16_t addr;
    struct Line *line;
    struct Instruction *next;
};

struct Line *parse_line(char *line);
long extract_addr(char *value);
void process_instruction(struct Instruction *this, struct Instruction *instructions);
uint16_t parse_instruction(struct Instruction *this, struct Instruction *instructions);

struct Line *
parse_line(char *line) {
    struct Line *parsed = (struct Line*) malloc(sizeof(struct Line));
    
    char *tokens = strtok(line, " ,\t\n\r");
    while (tokens != NULL) {
        if (strstr(tokens, ";") != NULL)
            break;

        if (!strcmp(parsed->keyword, "")) {
            for (int i = 0; i < sizeof(keywords) / sizeof(const char*); i++) {
                if (strcmp(tokens, keywords[i]) == 0) {
                    strcpy(parsed->keyword, tokens);
                    parsed->keyword_index = i;
                    break;
                }
            }
            if (!strcmp(parsed->keyword, "")) {
                strcpy(parsed->symbol, tokens);
            }
        } else {
            strcat(parsed->ops, tokens);
            strcat(parsed->ops, " ");
        }
        tokens = strtok(NULL, " ,\t");
    }
    return parsed;
}

long
extract_addr(char *value) {
    char val_pruned[max_ops_len];
    strcpy(val_pruned, value);
    uint16_t val = 0;
    if (strstr(value, "#-") != NULL) {
        strcpy(val_pruned, value+2);
        val = (uint16_t) strtol(val_pruned, NULL, 10);
        val = ~val + 1;
    } else if (strstr(value, "#") != NULL) {
        strcpy(val_pruned, value+1);
        val = (uint16_t) strtol(val_pruned, NULL, 10);
    } else if (strstr(value, "x") != NULL) {
        strcpy(val_pruned, value+1);
        val = (uint16_t) strtol(val_pruned, NULL, 16);
    } else if (strstr(value, "b") != NULL) {
        strcpy(val_pruned, value+1);
        val = (uint16_t) strtol(val_pruned, NULL, 2);
    } else {
        return -1;
    }
    return (long) val;
}

void
process_instruction(struct Instruction *this, struct Instruction *instructions) {
    switch (this->line->keyword_index) {
    case 32: /* .STRINGZ */
        /* insert another .STRINGZ with s[1:], return the 1st char as a .FILL */
        if (this->line->ops[0] == '\\') {
            char preceding;
            switch (this->line->ops[1]) {
            case 't':
                preceding = '\t';
                break;
            case 'n':
                preceding = '\n';
                break;
            case '"':
                preceding = '"';
                break;
            case '0':
                preceding = '\0';
                break;
            case '\\':
                preceding = '\\';
                break;
            }
            char trimmed_ops[max_ops_len];
            strncpy(trimmed_ops, this->line->ops+1, strlen(this->line->ops));
            trimmed_ops[0] = preceding;
            strcpy(this->line->ops, trimmed_ops);
        } else if (this->line->ops[0] == '"') {
            int end = 0;
            for (int i = 1; i < strlen(this->line->ops); i++) {
                if (this->line->ops[i] == '"' && this->line->ops[i-1] != '\\') {
                    end = i;
                    break;
                }
            }
            char new_ops[max_ops_len];
            strcpy(new_ops, "");
            strncpy(new_ops, this->line->ops+1, end-1);
            strcpy(this->line->ops, new_ops);
        }
        if (strlen(this->line->ops) > 1) {
            char strz[max_ops_len];
            strncpy(strz, this->line->ops+1, strlen(this->line->ops));
            
            struct Line *parsed = (struct Line*) malloc(sizeof(struct Line));
            strcpy(parsed->keyword, ".STRINGZ");
            parsed->keyword_index = 32;
            strcpy(parsed->ops, strz);
            
            struct Instruction *next = (struct Instruction*) malloc(sizeof(struct Instruction));
            next->addr = this->addr + 1;
            next->line = parsed;
            
            struct Instruction *offsetting = this->next;
            while (offsetting != NULL) {
                offsetting->addr++;
                offsetting = offsetting->next;
            }
            
            next->next = this->next;
            this->next = next;
        }
        return;
    default:
        return;
    }
}

uint16_t
parse_instruction(struct Instruction *this, struct Instruction *instructions) {
    uint16_t params[5];
    int param_types[5];
    for (int i = 0; i < 5; i++) {
        params[i] = 0;
        param_types[i] = P_UNUSED;
    }
    int p = 0;
    char tokenizing[max_ops_len];
    strcpy(tokenizing, this->line->ops);
    char *tokens = strtok(tokenizing, " ");
    while (tokens != NULL) {
        int resolved = 0;

        /* see if register */
        for (int i = 0; i < 8; i++) {
            if (!strcmp(regs[i], tokens)) {
                params[p] = i;
                param_types[p] = P_REG;
                resolved = 1;
                break;
            }
        }

        /* see if literal */
        if (!resolved) {
            long lit_value = extract_addr(tokens);
            if (lit_value != -1) {
                params[p] = (uint16_t) lit_value;
                param_types[p] = P_LIT;
                resolved = 1;
            }
        }

        /* otherwise search for label */
        if (!resolved) {
            struct Instruction *current = instructions;
            while (current != NULL) {
                if (!strcmp(current->line->symbol, tokens)) {
                    params[p] = current->addr - this->addr - 1;
                    param_types[p] = P_LAB;
                    resolved = 1;
                    break;
                }
                current = current->next;
            }
        }

        p++;
        tokens = strtok(NULL, " ");
    }
    
    switch (this->line->keyword_index) {
    case 0: /* LD */
        return (0b0010 << 12) | ((params[0] & 0b111) << 9) | (params[1] & 0b111111111);
    case 1: /* LDI */
        return (0b1010 << 12) | ((params[0] & 0b111) << 9) | (params[1] & 0b111111111);
    case 2: /* LDR */
        return (0b0110 << 12) | ((params[0] & 0b111) << 9) | ((params[1] & 0b111) << 6) | (params[2] & 0b111111);
    case 3: /* LEA */
        return (0b1110 << 12) | ((params[0] & 0b111) << 9) | (params[1] & 0b111111111);
    case 4: /* ST */
        return (0b0011 << 12) | ((params[0] & 0b111) << 9) | (params[1] & 0b111111111);
    case 5: /* STI */
        return (0b1011 << 12) | ((params[0] & 0b111) << 9) | (params[1] & 0b111111111);
    case 6: /* STR */
        return (0b0111 << 12) | ((params[0] & 0b111) << 9) | ((params[1] & 0b111) << 6) | (params[2] & 0b111111);
    case 7: /* ADD */
        switch (param_types[2]) {
        case P_REG:
            return (0b0001 << 12) | ((params[0] & 0b111) << 9) | ((params[1] & 0b111) << 6) | (0b000 << 3) | (params[2] & 0b111);
        case P_LIT:
            return (0b0001 << 12) | ((params[0] & 0b111) << 9) | ((params[1] & 0b111) << 6) | (0b1 << 5) | (params[2] & 0b11111);
        }
    case 8: /* AND */
        switch (param_types[2]) {
        case P_REG:
            return (0b0101 << 12) | ((params[0] & 0b111) << 9) | ((params[1] & 0b111) << 6) | (0b000 << 3) | (params[2] & 0b111);
        case P_LIT:
            return (0b0101 << 12) | ((params[0] & 0b111) << 9) | ((params[1] & 0b111) << 6) | (0b1 << 5) | (params[2] & 0b11111);
        }
    case 9: /* NOT */
        return (0b1001 << 12) | ((params[0] & 0b111) << 9) | ((params[1] & 0b111) << 6) | 0b111111;
    case 10: /* BR, BRnzp */
    case 17:
        return (0b0000 << 12) | (0b111 << 9) | (params[0] & 0b111111111);
    case 11: /* BRn */
        return (0b0000 << 12) | (0b100 << 9) | (params[0] & 0b111111111);
    case 12: /* BRz */
        return (0b0000 << 12) | (0b010 << 9) | (params[0] & 0b111111111);
    case 13: /* BRp */
        return (0b0000 << 12) | (0b001 << 9) | (params[0] & 0b111111111);
    case 14: /* BRnz */
        return (0b0000 << 12) | (0b110 << 9) | (params[0] & 0b111111111);
    case 15: /* BRzp */
        return (0b0000 << 12) | (0b011 << 9) | (params[0] & 0b111111111);
    case 16: /* BRnp */
        return (0b0000 << 12) | (0b101 << 9) | (params[0] & 0b111111111);
    case 18: /* JMP */
        return (0b1100 << 12) | (0b000 << 9) | ((params[0] & 0b111) << 6) | 0b000000;
    case 19: /* JSR */
        return (0b0100 << 12) | (0b1 << 11) | (params[0] & 0b11111111111);
    case 20: /* JSRR */
        return (0b0100 << 12) | (0b000 << 9) | ((params[0] & 0b111) << 6) | 0b000000;
    case 21: /* RET */
        return (0b1100 << 12) | (0b000111000000);
    case 22: /* RTI */
        return (0b1000 << 12) | (0b000000000000);
    case 23: /* GETC */
        return (0b1111 << 12) | (0x020);
    case 24: /* OUT */
        return (0b1111 << 12) | (0x021);
    case 25: /* PUTS */
        return (0b1111 << 12) | (0x022);
    case 26: /* IN */
        return (0b1111 << 12) | (0x023);
    case 27: /* PUTSP */
        return (0b1111 << 12) | (0x024);
    case 28: /* HALT */
        return (0b1111 << 12) | (0x025);
    case 34: /* DUMP */
        return (0b1111 << 12) | (0x026);
    case 35: /* BREAK */
        return (0b1111 << 12) | (0x027);
    case 29: /* .ORIG, .END */
    case 33:
        return 0;
    case 30: /* .FILL */
        return params[0];
    case 31: /* .BLKW */
        return 0;
    case 32: /* .STRINGZ */
        return (uint16_t) this->line->ops[0];
    }
    return 0;
}

int
main(int argc, char **argv) {
    char *in_path = NULL;
    char *out_path = NULL;
    
    int c;

    while ((c = getopt(argc, argv, "o:")) != -1) {
        switch (c) {
        case 'o':
            out_path = optarg;
            break;
        }
    }

    for (int i = optind; i < argc; i++) {
        in_path = argv[i];
    }

    FILE *in = fopen(in_path, "r");
    if (in == NULL) {
        printf("Unable to open file %s\n", in_path);
        return -1;
    }
    
    if (out_path == NULL)
        out_path = "out.obj";
    FILE *out = fopen(out_path, "wb");
    if (out == NULL) {
        printf("Unable to open file %s for writing\n", out_path);
    }
    
    struct Instruction *instructions = NULL;
    struct Instruction *last_instruction = NULL;

    char line_buffer[line_buffer_len];
    while (fgets(line_buffer, line_buffer_len, in) != NULL) {
        char line[line_buffer_len] = "";
        strncpy(line, line_buffer, strlen(line_buffer)-1);
        struct Line *parsed_line = parse_line(line);
        
        /* if (strcmp(parsed_line->keyword, "")) { */
        /*     printf("\nkw: %s\n", parsed_line->keyword); */
        /*     printf("sym: %s\n", parsed_line->symbol); */
        /*     printf("ops: %s\n", parsed_line->ops); */
        /* } */

        if (last_instruction == NULL) {
            if (strcmp(parsed_line->keyword, ".ORIG")) {
                continue;
            }
            instructions = (struct Instruction*) malloc(sizeof(struct Instruction));
            instructions->addr = (uint16_t) extract_addr(parsed_line->ops) - 1;
            instructions->line = parsed_line;
            instructions->next = NULL;

            last_instruction = instructions;
        } else if (strcmp(parsed_line->keyword, "")) {
            if (!strcmp(parsed_line->keyword, ".END")) {
                break;
            }
            struct Instruction *next = (struct Instruction*) malloc(sizeof(struct Instruction));
            next->addr = last_instruction->addr + 1;
            next->line = parsed_line;
            next->next = NULL;

            last_instruction->next = next;
            last_instruction = next;
        }
    }

    // drop .ORIG (mem leak lul)
    instructions = instructions->next;

    // process instructions
    struct Instruction *current = instructions;
    while (current != NULL) {
        process_instruction(current, instructions);
        current = current->next;
    }

    // with our lines+symbols loaded in, we can now go through and dump to the output
    current = instructions;
    uint16_t orig_addr = current->addr;
    char buff[] = {(orig_addr & 0xFF00) >> 8, orig_addr & 0x00FF};
    fwrite(buff, sizeof(char), sizeof(buff), out);
    while (current != NULL) {
        uint16_t instr = parse_instruction(current, instructions);
        buff[0] = (instr & 0xFF00) >> 8;
        buff[1] = instr & 0x00FF;
        fwrite(buff, sizeof(char), sizeof(buff), out);
        /* parse_instruction(current, instructions); */
        /* puts(""); */
        /* fprintf(out, "%x\n", current->addr); */
        current = current->next;
    }

    return 0;
}

