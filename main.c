#include <stdio.h>
#include <stdlib.h>
#include "lc3.h"

void load_file(FILE *fp, LC3 *l);

void
load_file(FILE *fp, LC3 *l) {
    fseek(fp, 0, SEEK_END);
    long filelen = ftell(fp);
    rewind(fp);

    char *buffer = (char*) malloc(2 * sizeof(char));
    uint16_t memptr = 0;
    for (int i = 0; i < filelen; i += 2) {
        fread(buffer, sizeof(char), 2, fp);
        uint16_t val = (((uint8_t) buffer[0]) << 8) | ((uint8_t) buffer[1]);
        if (i == 0) {
            memptr = val;
            l->reg[R_PC] = val;
        } else {
            l->mem[memptr] = val;
            memptr++;
        }
    }
}

int
main(int argc, char **argv) {
    LC3 *l;
    l = malloc(sizeof(LC3));

    for (int i = 1; i < argc; i++) {
        char *in_path = argv[i];
        
        FILE *in = fopen(in_path, "rb");
        if (in == NULL) {
            printf("Unable to open file %s\n", in_path);
            return -1;
        }
        
        load_file(in, l);
    }
    
    l->running = 1;

    run_lc3(l);
}
