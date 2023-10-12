#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#define EOF_ENCOUNTERED "Invalid input file: EOF encountered\n"

const char* const reg_w0[8] = {
    "al", "cl", "dl", "bl",
    "ah", "ch", "dh", "bh",
};
const char* const reg_w1[8] = {
    "ax", "cx", "dx", "bx",
    "sp", "bp", "si", "di",
};

const char* lookup_register(uint8_t w, uint8_t reg) {
    assert(reg < 8);
    return w ? reg_w1[reg] : reg_w0[reg];
}

const char* const effective_address_calculations[8] = {
    "bx + si", "bx + di", "bp + si", "bp + di",
    "si", "di", "bp", "bx",
};
const char* lookup_effective_address_calculation(uint8_t rm) {
    assert(rm < 8);
    return effective_address_calculations[rm];
}

int read_byte(FILE *input, uint8_t *byte) {
    assert(byte);

    int read_c = fgetc(input);
    if (read_c == EOF) return 1;

    *byte = read_c;
    return 0;
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

    printf("; %s disassembly:\n", argv[1]);
    printf("bits 16\n");

    uint8_t a;
    while (!read_byte(input, &a)) {
        if ((a & 0b11111100) == 0b10001000) {
            // MOV: Registry/memory to/from registry

            uint8_t b;
            if (read_byte(input, &b)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }

            uint8_t d = (a & (1 << 1)) >> 1;
            uint8_t w = a & 1;
            uint8_t mod = (b & 0b11000000) >> 6;
            uint8_t reg = (b & 0b00111000) >> 3;
            uint8_t rm = b & 0b111;

            const char *looked_reg = lookup_register(w, reg);
            const char *looked_rm = mod == 3 ? lookup_register(w, rm) : lookup_effective_address_calculation(rm);

            uint8_t displacement_bytes = 0;
            if (mod == 1) {
                displacement_bytes = 1;
            } else if (mod == 2 || (mod == 0 && rm == 0b110)) {
                displacement_bytes = 2;
            }

            uint8_t displacement_lo = 0;
            if (displacement_bytes && read_byte(input, &displacement_lo)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            uint8_t displacement_hi = 0;
            if (displacement_bytes == 2 && read_byte(input, &displacement_hi)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            int16_t displacement = displacement_bytes == 2 ? displacement_lo | (displacement_hi << 8) : (int8_t)displacement_lo;
            char displacement_sign = displacement > 0 ? '+' : '-';

            switch (mod) {
                case 0: {
                    if (rm == 0b110) {
                        printf("mov %s, [%d]\n", looked_reg, displacement);
                    } else {
                        if (d) {
                            printf("mov %s, [%s]\n", looked_reg, looked_rm);
                        } else {
                            printf("mov [%s], %s\n", looked_rm , looked_reg);
                        }
                    }
                    break;
                }
                case 1:
                case 2: {
                    if (d) {
                        if (displacement) {
                            printf("mov %s, [%s %c %d]\n", looked_reg, looked_rm, displacement_sign, abs(displacement));
                        } else {
                            printf("mov %s, [%s]\n", looked_reg, looked_rm);
                        }
                    } else {
                        if (displacement) {
                            printf("mov [%s %c %d], %s\n", looked_rm, displacement_sign, abs(displacement), looked_reg);
                        } else {
                            printf("mov [%s], %s\n", looked_rm, looked_reg);
                        }
                    }
                    break;
                }
                case 3: {
                    printf("mov %s, %s\n", d ? looked_reg : looked_rm, d ? looked_rm : looked_reg);
                    break;
                }
                default:
                    fprintf(stderr, "Unsupported mod %d\n", mod);
                    goto cleanup;
            }
        } else if ((a & 0b11110000) == 0b10110000) {
            // MOV: Immediate to register

            uint8_t w = !!(a & 0b1000);
            uint8_t reg = a & 0b111;

            uint8_t data_lo, data_hi = 0;
            if (read_byte(input, &data_lo)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            if (w && read_byte(input, &data_hi)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }

            uint16_t data = data_lo | (data_hi << 8);
            printf("mov %s, %d\n", lookup_register(w, reg), data);
        } else if ((a & 0b11111110) == 0b11000110) {
            // MOV: Immediate to register/memory

            uint8_t b;
            if (read_byte(input, &b)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }

            uint8_t w = a & 1;
            uint8_t mod = (b & 0b11000000) >> 6;
            uint8_t rm = b & 0b111;

            const char *looked_rm = mod == 3 ? lookup_register(w, rm) : lookup_effective_address_calculation(rm);

            if (mod == 0 && rm == 0b110) {
                fprintf(stderr, "Mov with direct address not supported\n");
                goto cleanup;
            }

            uint8_t displacement_bytes = mod == 3 ? 0 : mod;

            uint8_t displacement_lo = 0;
            if (displacement_bytes && read_byte(input, &displacement_lo)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            uint8_t displacement_hi = 0;
            if (displacement_bytes == 2 && read_byte(input, &displacement_hi)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            int16_t displacement = displacement_bytes == 2 ? displacement_lo | (displacement_hi << 8) : (int8_t)displacement_lo;
            char displacement_sign = displacement > 0 ? '+' : '-';

            uint8_t data_lo, data_hi = 0;
            if (read_byte(input, &data_lo)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            if (w && read_byte(input, &data_hi)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            uint16_t data = data_lo | (data_hi << 8);
            const char *data_keyword = w ? "word" : "byte";

            printf("mov ");
            switch (mod) {
                case 0:
                    printf("[%s]", looked_rm);
                    break;
                case 1:
                case 2:
                    if (displacement) {
                        printf("[%s %c %d]", looked_rm, displacement_sign, abs(displacement));
                    } else {
                        printf("[%s]", looked_rm);
                    }
                    break;
                case 3:
                    printf("%s", looked_rm);
                    break;
                default:
                    fprintf(stderr, "Unsupported mod %d\n", mod);
                    goto cleanup;
            }
            printf(", %s %d\n", data_keyword, data);
        } else if ((a & 0b11111110) == 0b10100000) {
            // Mov: Memory to accumulator

            uint8_t w = a & 1;

            uint8_t address_lo, address_hi = 0;
            if (read_byte(input, &address_lo)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            if (w && read_byte(input, &address_hi)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            int16_t address = w ? address_lo | (address_hi << 8) : (int8_t)address_lo;

            printf("mov ax, [%d]\n", address);
        } else if ((a & 0b11111110) == 0b10100010) {
            // Mov: Accumulator to memory

            uint8_t w = a & 1;

            uint8_t address_lo, address_hi = 0;
            if (read_byte(input, &address_lo)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            if (w && read_byte(input, &address_hi)) {
                fprintf(stderr, EOF_ENCOUNTERED);
                goto cleanup;
            }
            int16_t address = w ? address_lo | (address_hi << 8) : (int8_t)address_lo;

            printf("mov [%d], ax\n", address);
        } else {
            fprintf(stderr, "Unsupported opcode 0x%X\n", a);
            goto cleanup;
        }
    }

    ret = 0;

cleanup:
    fclose(input);
    return ret;
}
