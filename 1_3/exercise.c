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

const char* const arithmetic_operations[8] = {
    "add", "b", "c", "d",
    "e", "sub", "g", "cmp",
};

const char* const jmp_instructions[16] = {
    "jo", "jno", "jb", "jnb", "je", "jnz", "jbe", "ja",
    "js", "jns", "jp", "jnp", "jl", "jnl", "jle", "jg",
};

const char* const loop_instructions[4] = {
    "loopnz", "loopz", "loop", "jcxz",
};

int read_byte(FILE *input, uint8_t *byte) {
    assert(byte);

    int read_c = fgetc(input);
    if (read_c == EOF) return -1;

    *byte = read_c;
    return 0;
}

int read_displacement(FILE *input, uint8_t displacement_bytes, uint8_t sign_extension, int32_t *result) {
    assert(result && displacement_bytes <= 2);
    uint8_t displacement_lo = 0;
    if (displacement_bytes && read_byte(input, &displacement_lo)) {
        fprintf(stderr, EOF_ENCOUNTERED);
        return -1;
    }
    uint8_t displacement_hi = 0;
    if (displacement_bytes == 2 && read_byte(input, &displacement_hi)) {
        fprintf(stderr, EOF_ENCOUNTERED);
        return -1;
    }

    if (displacement_bytes == 2) {
        *result = displacement_lo | (displacement_hi << 8);
    } else {
        *result = sign_extension ? (int8_t)displacement_lo : displacement_lo;
    }

    return 0;
}

int read_data(FILE *input, uint8_t wide, uint16_t *result) {
    assert(result);
    uint8_t data_lo, data_hi = 0;
    if (read_byte(input, &data_lo)) {
        fprintf(stderr, EOF_ENCOUNTERED);
        return -1;
    }
    if (wide && read_byte(input, &data_hi)) {
        fprintf(stderr, EOF_ENCOUNTERED);
        return -1;
    }
    *result = data_lo | (data_hi << 8);
    return 0;
}

int decode_rm_with_register(FILE *input, const char *op, uint8_t a) {
    uint8_t b;
    if (read_byte(input, &b)) {
        fprintf(stderr, EOF_ENCOUNTERED);
        return -1;
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

    int32_t displacement = 0;
    if (read_displacement(input, displacement_bytes, 1, &displacement)) {
        return -1;
    }
    char displacement_sign = displacement > 0 ? '+' : '-';

    printf("%s ", op);
    switch (mod) {
        case 0: 
            if (rm == 0b110) {
                printf("%s, [%d]\n", looked_reg, displacement);
            } else {
                if (d) {
                    printf("%s, [%s]\n", looked_reg, looked_rm);
                } else {
                    printf("[%s], %s\n", looked_rm , looked_reg);
                }
            }
            break;
        case 1:
        case 2:
            if (d) {
                if (displacement) {
                    printf("%s, [%s %c %d]\n", looked_reg, looked_rm, displacement_sign, abs(displacement));
                } else {
                    printf("%s, [%s]\n", looked_reg, looked_rm);
                }
            } else {
                if (displacement) {
                    printf("[%s %c %d], %s\n", looked_rm, displacement_sign, abs(displacement), looked_reg);
                } else {
                    printf("[%s], %s\n", looked_rm, looked_reg);
                }
            }
            break;
        case 3:
            printf("%s, %s\n", d ? looked_reg : looked_rm, d ? looked_rm : looked_reg);
            break;
        default:
            fprintf(stderr, "Unsupported mod %d\n", mod);
            return -1;
    }

    return 0;
}

int decode_immediate_to_rm(FILE *input, uint8_t is_mov, uint8_t a) {
    uint8_t b;
    if (read_byte(input, &b)) {
        fprintf(stderr, EOF_ENCOUNTERED);
        return -1;
    }

    uint8_t s = !!(a & 0b10);
    uint8_t w = a & 1;
    uint8_t mod = (b & 0b11000000) >> 6;
    uint8_t op = (b & 0b00111000) >> 3;
    uint8_t rm = b & 0b111;

    uint8_t is_direct_address = mod == 0 && rm == 0b110;
    const char *looked_rm = mod == 3 ? lookup_register(w, rm) : lookup_effective_address_calculation(rm);

    uint8_t displacement_bytes = mod == 3 ? 0 : mod;
    if (is_direct_address) {
        displacement_bytes = 2;
    }
    int32_t displacement = 0;
    if (read_displacement(input, displacement_bytes, s, &displacement)) {
        return -1;
    }
    char displacement_sign = displacement > 0 ? '+' : '-';

    uint16_t data;
    if (read_data(input, is_mov ? w : !s && w, &data)) {
        return -1;
    }

    printf("%s ", is_mov ? "mov" : arithmetic_operations[op]);
    switch (mod) {
        case 0:
            if (is_direct_address) {
                printf("[%d]", displacement);
            } else {
                printf("[%s]", looked_rm);
            }
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
            return -1;
    }
    printf(", %s %d\n", w ? "word" : "byte", data);

    return 0;
}

int decode_ip_inc(FILE *input, uint8_t a, uint8_t bitmask, const char* const *instructions, uint16_t instructions_length) {
    int8_t ip_inc;
    if (read_byte(input, (uint8_t*)&ip_inc)) {
        fprintf(stderr, EOF_ENCOUNTERED);
        return -1;
    }

    uint8_t i = a & bitmask;
    if (i >= instructions_length) {
        fprintf(stderr, "Invalid instruction with first byte 0x%X\n", a);
        return -1;
    }

    int16_t adjusted_ip_inc = ip_inc + 2;
    printf("%s $%s%d\n", instructions[i], adjusted_ip_inc < 0 ? "" : "+", adjusted_ip_inc);

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <input_file_name>\n", argc > 0 ? argv[0] : "a");
        return -1;
    }

    int ret = -1;

    FILE *input = fopen(argv[1], "rb");
    if (!input) {
        fprintf(stderr, "Couldn't open file %s\n", argv[1]);
        return -1;
    }

    printf("; %s disassembly:\n", argv[1]);
    printf("bits 16\n");

    uint8_t a;
    while (!read_byte(input, &a)) {
        if ((a & 0b11111100) == 0b10001000) {
            // MOV: Registry/memory to/from registry
            if (decode_rm_with_register(input, "mov", a)) {
                goto cleanup;
            }
        } else if ((a & 0b11111110) == 0b11000110) {
            // MOV: Immediate to register/memory
            if (decode_immediate_to_rm(input, 1, a)) {
                goto cleanup;
            }
        } else if ((a & 0b11110000) == 0b10110000) {
            // MOV: Immediate to register

            uint8_t w = !!(a & 0b1000);
            uint8_t reg = a & 0b111;

            uint16_t data;
            if (read_data(input, w, &data)) {
                goto cleanup;
            }
            printf("mov %s, %d\n", lookup_register(w, reg), data);
        } else if ((a & 0b11111110) == 0b10100000) {
            // Mov: Memory to accumulator

            uint8_t w = a & 1;
            int32_t address;
            if (read_displacement(input, 1 + w, 1, &address)) {
                goto cleanup;
            }

            printf("mov ax, [%d]\n", address);
        } else if ((a & 0b11111110) == 0b10100010) {
            // Mov: Accumulator to memory

            uint8_t w = a & 1;
            int32_t address;
            if (read_displacement(input, 1 + w, 1, &address)) {
                goto cleanup;
            }

            printf("mov [%d], ax\n", address);
        } else if ((a & 0b11000100) == 0) {
            uint8_t op = (a & 0b00111000) >> 3;
            if (decode_rm_with_register(input, arithmetic_operations[op], a)) {
                goto cleanup;
            }
        } else if ((a & 0b11111100) == 0b10000000) {
            if (decode_immediate_to_rm(input, 0, a)) {
                goto cleanup;
            }
        } else if ((a & 0b11000110) == 0b00000100) {
            // Immediate to accumulator

            uint8_t op = (a & 0b00111000) >> 3;
            uint8_t w = a & 1;

            uint16_t data;
            if (read_data(input, w, &data)) {
                goto cleanup;
            }
            printf("%s %s, %d\n", arithmetic_operations[op], w ? "ax" : "al", data);
        } else if ((a & 0b11110000) == 0x70) {
            if (decode_ip_inc(input, a, 0b1111, jmp_instructions, sizeof(jmp_instructions) / sizeof(jmp_instructions[0]))) {
                goto cleanup;
            }
        } else if ((a & 0b11111100) == 0b11100000) {
            if (decode_ip_inc(input, a, 0b11, loop_instructions, sizeof(loop_instructions) / sizeof(loop_instructions[0]))) {
                goto cleanup;
            }
        } else {
            fprintf(stderr, "Unsupported first byte of instruction 0x%X\n", a);
            goto cleanup;
        }
    }

    ret = 0;

cleanup:
    fclose(input);
    return ret;
}
