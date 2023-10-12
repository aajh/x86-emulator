#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define MAX_REG 0b111

const char* const reg_w0[MAX_REG + 1] = {
    "al", "cl", "dl", "bl",
    "ah", "ch", "dh", "bh",
};
const char* const reg_w1[MAX_REG + 1] = {
    "ax", "cx", "dx", "bx",
    "sp", "bp", "si", "di",
};

const char* lookup_register(uint8_t w, uint8_t reg) {
    assert(reg <= MAX_REG);
    return w ? reg_w1[reg] : reg_w0[reg];
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: a <input_file_name>\n");
        return -1;
    }

    int ret = -1;

    FILE *input = fopen(argv[1], "rb");
    if (!input) {
        fprintf(stderr, "Coudln't open file %s\n", argv[1]);
        return -1;
    }

    printf(";%s\n", argv[1]);
    printf("bits 16\n");

    uint8_t a, b;
    int c = EOF;
    while ((c = fgetc(input)) != EOF) {
        a = c;
        if ((c = fgetc(input)) == EOF) {
            fprintf(stderr, "Invalid input file: EOF encountered\n");
            goto cleanup;
        }
        b = c;

        uint8_t opcode = (a & 0b11111100) >> 2;
        if (opcode != 0b100010) {
            fprintf(stderr, "Invalid opcode 0x%X\n", opcode);
            goto cleanup;
        }

        uint8_t d = (a & (1 << 1)) >> 1;
        uint8_t w = a & 1;

        uint8_t mod = (b & 0b11000000) >> 6;
        if (mod != 3) {
            fprintf(stderr, "Unsupported mod %d\n", mod);
            goto cleanup;
        }

        uint8_t reg = (b & 0b00111000) >> 3;
        uint8_t rm = b & 0b111;

        if (d) {
            printf("mov %s, %s\n", lookup_register(w, reg), lookup_register(w, rm));
        } else {
            printf("mov %s, %s\n", lookup_register(w, rm), lookup_register(w, reg));
        }
    }

    ret = 0;

cleanup:
    fclose(input);
    return ret;
}
