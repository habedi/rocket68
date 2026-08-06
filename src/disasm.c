/**
 * @file disasm.c
 * @brief 68000 instruction disassembler implementation.
 */
#if defined(__STDC_LIB_EXT1__)
#define __STDC_WANT_LIB_EXT1__ 1
#endif

#include "disasm.h"

#include <stdio.h>
#include <string.h>

#ifndef __STDC_LIB_EXT1__
#define snprintf_s(buf, sz, fmt, ...) snprintf(buf, sz, fmt, ##__VA_ARGS__)
#endif

/*
 * A disassembled line is built from three fixed-size scratch buffers, one for an
 * effective address, one for a mnemonic, and one for an argument list. The longest
 * string any addressing mode can produce is "$FFFFFFFF(PC)" at 13 characters, and the
 * longest mnemonic is "MOVEM.L" at 7, so both buffers carry about 10 characters of
 * slack. `tests/test_disasm_bounds.c` enforces those maxima over the whole opcode
 * space.
 */
#define DISASM_EA_SIZE 24
#define DISASM_OP_SIZE 32
#define DISASM_ARGS_SIZE 64

/*
 * The widest argument list is two effective addresses joined by ", ". Keeping the
 * argument buffer at least that large lets the compiler prove that no formatting
 * call can truncate it, which avoids a -Wformat-overflow/-Wformat-truncation diagnostic
 * under GCC at -O2 and above.
 */
_Static_assert(2 * DISASM_EA_SIZE + sizeof(", ") <= DISASM_ARGS_SIZE,
               "args buffer must hold two effective addresses plus a separator");

/* Side-effect-free instruction peek: no wait states, no faults, no cycles. */
static u16 peek_word(M68kCpu* cpu, u32 pc) {
    u32 address = pc & 0x00FFFFFFu;
    if (cpu->read16_cb) {
        return cpu->read16_cb(cpu, address);
    }
    if (cpu->memory && address + 1 < cpu->memory_size) {
        return (u16)((cpu->memory[address] << 8) | cpu->memory[address + 1]);
    }
    return 0;
}

static u32 peek_long(M68kCpu* cpu, u32 pc) {
    return ((u32)peek_word(cpu, pc) << 16) | peek_word(cpu, pc + 2);
}

static const char* size_str(int size_code) {
    switch (size_code) {
        case 0:
            return ".B";
        case 1:
            return ".W";
        case 2:
            return ".L";
        case 3:
            return ".?";
        default:
            return ".?";
    }
}

static int disasm_ea(M68kCpu* cpu, u32 base_pc, int mode, int reg, int size, char* buf) {
    int bytes = 0;

    switch (mode) {
        case 0:
            snprintf(buf, DISASM_EA_SIZE, "D%d", reg);
            break;
        case 1:
            snprintf(buf, DISASM_EA_SIZE, "A%d", reg);
            break;
        case 2:
            snprintf(buf, DISASM_EA_SIZE, "(A%d)", reg);
            break;
        case 3:
            snprintf(buf, DISASM_EA_SIZE, "(A%d)+", reg);
            break;
        case 4:
            snprintf(buf, DISASM_EA_SIZE, "-(A%d)", reg);
            break;
        case 5: {
            s16 disp = (s16)peek_word(cpu, base_pc);
            snprintf(buf, DISASM_EA_SIZE, "%d(A%d)", disp, reg);
            bytes = 2;
            break;
        }
        case 6: {
            u16 ext = peek_word(cpu, base_pc);
            s8 disp = (s8)(ext & 0xFF);
            int idx_reg = (ext >> 12) & 0x7;
            bool is_a_reg = (ext >> 15) & 1;
            bool is_long = (ext >> 11) & 1;
            snprintf(buf, DISASM_EA_SIZE, "%d(A%d,%c%d.%c)", disp, reg, is_a_reg ? 'A' : 'D', idx_reg,
                    is_long ? 'L' : 'W');
            bytes = 2;
            break;
        }
        case 7: {
            switch (reg) {
                case 0: {
                    u16 addr = peek_word(cpu, base_pc);
                    snprintf(buf, DISASM_EA_SIZE, "$%04X.W", addr);
                    bytes = 2;
                    break;
                }
                case 1: {
                    u32 addr = peek_long(cpu, base_pc);
                    snprintf(buf, DISASM_EA_SIZE, "$%08X.L", addr);
                    bytes = 4;
                    break;
                }
                case 2: {
                    s16 disp = (s16)peek_word(cpu, base_pc);
                    snprintf(buf, DISASM_EA_SIZE, "$%X(PC)", base_pc + disp);

                    bytes = 2;
                    break;
                }
                case 3: {
                    u16 ext = peek_word(cpu, base_pc);
                    s8 disp = (s8)(ext & 0xFF);
                    int idx_reg = (ext >> 12) & 0x7;
                    bool is_a_reg = (ext >> 15) & 1;
                    bool is_long = (ext >> 11) & 1;
                    snprintf(buf, DISASM_EA_SIZE, "%d(PC,%c%d.%c)", disp, is_a_reg ? 'A' : 'D', idx_reg,
                            is_long ? 'L' : 'W');
                    bytes = 2;
                    break;
                }
                case 4: {
                    if (size == SIZE_LONG) {
                        u32 val = peek_long(cpu, base_pc);
                        snprintf(buf, DISASM_EA_SIZE, "#$%X", val);
                        bytes = 4;
                    } else if (size == SIZE_BYTE) {
                        u16 val = peek_word(cpu, base_pc);
                        snprintf(buf, DISASM_EA_SIZE, "#$%02X", val & 0xFF);
                        bytes = 2;
                    } else {
                        u16 val = peek_word(cpu, base_pc);
                        snprintf(buf, DISASM_EA_SIZE, "#$%04X", val);
                        bytes = 2;
                    }
                    break;
                }
                default:
                    snprintf(buf, DISASM_EA_SIZE, "???");
                    break;
            }
            break;
        }
    }
    return bytes;
}

static int decode_move(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args) {
    int bytes = 2;
    int size_code = (opcode >> 12) & 0x3;
    int size;
    const char* sz_str;

    switch (size_code) {
        case 1:
            size = SIZE_BYTE;
            sz_str = ".B";
            break;
        case 3:
            size = SIZE_WORD;
            sz_str = ".W";
            break;
        case 2:
            size = SIZE_LONG;
            sz_str = ".L";
            break;
        default:
            return 0;
    }

    snprintf(op, DISASM_OP_SIZE, "MOVE%s", sz_str);

    int dest_reg = (opcode >> 9) & 0x7;
    int dest_mode = (opcode >> 6) & 0x7;
    int src_mode = (opcode >> 3) & 0x7;
    int src_reg = opcode & 0x7;

    char src_buf[DISASM_EA_SIZE];
    int src_bytes = disasm_ea(cpu, pc + bytes, src_mode, src_reg, size, src_buf);
    bytes += src_bytes;

    char dest_buf[DISASM_EA_SIZE];
    int dest_bytes = disasm_ea(cpu, pc + bytes, dest_mode, dest_reg, size, dest_buf);
    bytes += dest_bytes;

    snprintf(args, DISASM_ARGS_SIZE, "%s, %s", src_buf, dest_buf);
    return bytes;
}

static int decode_branch(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args) {
    int cond = (opcode >> 8) & 0xF;
    s8 d8 = (s8)(opcode & 0xFF);
    int bytes = 2;

    const char* cc_names[] = {"RA", "SR", "HI", "LS", "CC", "CS", "NE", "EQ",
                              "VC", "VS", "PL", "MI", "GE", "LT", "GT", "LE"};

    if (cond == 1)
        snprintf(op, DISASM_OP_SIZE, "BSR");
    else if (cond == 0)
        snprintf(op, DISASM_OP_SIZE, "BRA");
    else
        snprintf(op, DISASM_OP_SIZE, "B%s", cc_names[cond]);

    if (d8 == 0) {
        s16 d16 = (s16)peek_word(cpu, pc + 2);
        snprintf(args, DISASM_ARGS_SIZE, "$%X", pc + 2 + d16);
        bytes += 2;
    } else if (d8 == -1) {
        snprintf(args, DISASM_ARGS_SIZE, "$%X", pc + 2 + d8);
    } else {
        snprintf(args, DISASM_ARGS_SIZE, "$%X", pc + 2 + d8);
    }

    return bytes;
}

static int decode_std_arith(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args,
                            const char* name_base) {
    int reg = (opcode >> 9) & 0x7;
    int opmode = (opcode >> 6) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int ea_reg = opcode & 0x7;
    int bytes = 2;
    int size = SIZE_WORD;
    const char* sz = ".W";

    bool dn_dest = true;

    switch (opmode) {
        case 0:
            size = SIZE_BYTE;
            sz = ".B";
            break;
        case 1:
            size = SIZE_WORD;
            sz = ".W";
            break;
        case 2:
            size = SIZE_LONG;
            sz = ".L";
            break;
        case 4:
            size = SIZE_BYTE;
            sz = ".B";
            dn_dest = false;
            break;
        case 5:
            size = SIZE_WORD;
            sz = ".W";
            dn_dest = false;
            break;
        case 6:
            size = SIZE_LONG;
            sz = ".L";
            dn_dest = false;
            break;
        default:
            return 0;
    }

    snprintf(op, DISASM_OP_SIZE, "%s%s", name_base, sz);

    char ea_buf[DISASM_EA_SIZE];
    int ea_bytes = disasm_ea(cpu, pc + bytes, mode, ea_reg, size, ea_buf);
    bytes += ea_bytes;

    if (dn_dest) {
        snprintf(args, DISASM_ARGS_SIZE, "%s, D%d", ea_buf, reg);
    } else {
        snprintf(args, DISASM_ARGS_SIZE, "D%d, %s", reg, ea_buf);
    }

    return bytes;
}

static int decode_imm_arith(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args,
                            const char* name) {
    int bytes = 2;
    int size_code = (opcode >> 6) & 0x3;
    int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);

    snprintf(op, DISASM_OP_SIZE, "%s%s", name, size_str(size_code));

    char imm_buf[DISASM_EA_SIZE];
    int imm_bytes = disasm_ea(cpu, pc + bytes, 7, 4, size, imm_buf);
    bytes += imm_bytes;

    char ea_buf[DISASM_EA_SIZE];
    int ea_mode = (opcode >> 3) & 0x7;
    int ea_reg = opcode & 0x7;
    int ea_bytes = disasm_ea(cpu, pc + bytes, ea_mode, ea_reg, size, ea_buf);
    bytes += ea_bytes;

    snprintf(args, DISASM_ARGS_SIZE, "%s, %s", imm_buf, ea_buf);
    return bytes;
}

int m68k_disasm(M68kCpu* cpu, u32 pc, char* buffer, int buf_size) {
    u16 opcode = peek_word(cpu, pc);
    char op[DISASM_OP_SIZE] = "???";
    char args[DISASM_ARGS_SIZE] = "";
    int len = 2;

    if ((opcode & 0xF000) == 0x0000) {
        int top4 = (opcode >> 8) & 0xF;

        if (top4 == 0x08) {
            static const char* bit_names[] = {"BTST", "BCHG", "BCLR", "BSET"};
            int subop = (opcode >> 6) & 0x3;
            snprintf(op, DISASM_OP_SIZE, "%s", bit_names[subop]);

            u16 bit_num = peek_word(cpu, pc + 2);
            len += 2;
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes =
                disasm_ea(cpu, pc + len, (opcode >> 3) & 7, opcode & 7, SIZE_BYTE, ea_buf);
            len += ea_bytes;
            snprintf(args, DISASM_ARGS_SIZE, "#%d, %s", bit_num & 0xFF, ea_buf);
        }

        else if (opcode & 0x0100) {
            int dn = (opcode >> 9) & 7;
            int mode = (opcode >> 3) & 7;
            if (mode == 1) {
                int opmode = (opcode >> 6) & 7;
                s16 disp = (s16)peek_word(cpu, pc + 2);
                len += 2;
                int areg = opcode & 7;
                if (opmode == 4 || opmode == 5) {
                    snprintf(op, DISASM_OP_SIZE, "MOVEP%s", (opmode == 5) ? ".L" : ".W");
                    snprintf(args, DISASM_ARGS_SIZE, "%d(A%d), D%d", disp, areg, dn);
                } else {
                    snprintf(op, DISASM_OP_SIZE, "MOVEP%s", (opmode == 7) ? ".L" : ".W");
                    snprintf(args, DISASM_ARGS_SIZE, "D%d, %d(A%d)", dn, disp, areg);
                }
            } else {
                static const char* bit_names[] = {"BTST", "BCHG", "BCLR", "BSET"};
                int subop = (opcode >> 6) & 0x3;
                snprintf(op, DISASM_OP_SIZE, "%s", bit_names[subop]);
                char ea_buf[DISASM_EA_SIZE];
                int ea_bytes = disasm_ea(cpu, pc + 2, mode, opcode & 7, SIZE_BYTE, ea_buf);
                len += ea_bytes;
                snprintf(args, DISASM_ARGS_SIZE, "D%d, %s", dn, ea_buf);
            }
        }

        else if (opcode == 0x003C) {
            snprintf(op, DISASM_OP_SIZE, "ORI");
            snprintf(args, DISASM_ARGS_SIZE, "#$%02X, CCR", peek_word(cpu, pc + 2) & 0xFF);
            len += 2;
        } else if (opcode == 0x007C) {
            snprintf(op, DISASM_OP_SIZE, "ORI");
            snprintf(args, DISASM_ARGS_SIZE, "#$%04X, SR", peek_word(cpu, pc + 2));
            len += 2;
        } else if (opcode == 0x023C) {
            snprintf(op, DISASM_OP_SIZE, "ANDI");
            snprintf(args, DISASM_ARGS_SIZE, "#$%02X, CCR", peek_word(cpu, pc + 2) & 0xFF);
            len += 2;
        } else if (opcode == 0x027C) {
            snprintf(op, DISASM_OP_SIZE, "ANDI");
            snprintf(args, DISASM_ARGS_SIZE, "#$%04X, SR", peek_word(cpu, pc + 2));
            len += 2;
        } else if (opcode == 0x0A3C) {
            snprintf(op, DISASM_OP_SIZE, "EORI");
            snprintf(args, DISASM_ARGS_SIZE, "#$%02X, CCR", peek_word(cpu, pc + 2) & 0xFF);
            len += 2;
        } else if (opcode == 0x0A7C) {
            snprintf(op, DISASM_OP_SIZE, "EORI");
            snprintf(args, DISASM_ARGS_SIZE, "#$%04X, SR", peek_word(cpu, pc + 2));
            len += 2;
        }

        else {
            switch (top4) {
                case 0x00:
                    len = decode_imm_arith(cpu, pc, opcode, op, args, "ORI");
                    break;
                case 0x02:
                    len = decode_imm_arith(cpu, pc, opcode, op, args, "ANDI");
                    break;
                case 0x04:
                    len = decode_imm_arith(cpu, pc, opcode, op, args, "SUBI");
                    break;
                case 0x06:
                    len = decode_imm_arith(cpu, pc, opcode, op, args, "ADDI");
                    break;
                case 0x0A:
                    len = decode_imm_arith(cpu, pc, opcode, op, args, "EORI");
                    break;
                case 0x0C:
                    len = decode_imm_arith(cpu, pc, opcode, op, args, "CMPI");
                    break;
                default:
                    break;
            }
        }
    }

    else if ((opcode & 0xC000) == 0x0000 && (opcode & 0xF000) != 0) {
        len = decode_move(cpu, pc, opcode, op, args);
    }

    else if ((opcode & 0xF000) == 0x4000) {
        if (opcode == 0x4E70) {
            snprintf(op, DISASM_OP_SIZE, "RESET");
        } else if (opcode == 0x4E71) {
            snprintf(op, DISASM_OP_SIZE, "NOP");
        } else if (opcode == 0x4E72) {
            snprintf(op, DISASM_OP_SIZE, "STOP");
            snprintf(args, DISASM_ARGS_SIZE, "#$%04X", peek_word(cpu, pc + 2));
            len += 2;
        } else if (opcode == 0x4E73) {
            snprintf(op, DISASM_OP_SIZE, "RTE");
        } else if (opcode == 0x4E75) {
            snprintf(op, DISASM_OP_SIZE, "RTS");
        } else if (opcode == 0x4E76) {
            snprintf(op, DISASM_OP_SIZE, "TRAPV");
        } else if (opcode == 0x4E77) {
            snprintf(op, DISASM_OP_SIZE, "RTR");
        }

        else if ((opcode & 0xFFF0) == 0x4E40) {
            snprintf(op, DISASM_OP_SIZE, "TRAP");
            snprintf(args, DISASM_ARGS_SIZE, "#%d", opcode & 0xF);
        }

        else if ((opcode & 0xFFF8) == 0x4E48) {
            snprintf(op, DISASM_OP_SIZE, "BKPT");
            snprintf(args, DISASM_ARGS_SIZE, "#%d", opcode & 7);
        }

        else if ((opcode & 0xFFF8) == 0x4E50) {
            snprintf(op, DISASM_OP_SIZE, "LINK");
            s16 disp = (s16)peek_word(cpu, pc + 2);
            len += 2;
            snprintf(args, DISASM_ARGS_SIZE, "A%d, #%d", opcode & 7, disp);
        }

        else if ((opcode & 0xFFF8) == 0x4E58) {
            snprintf(op, DISASM_OP_SIZE, "UNLK");
            snprintf(args, DISASM_ARGS_SIZE, "A%d", opcode & 7);
        }

        else if ((opcode & 0xFFF0) == 0x4E60) {
            int reg = opcode & 7;
            if (opcode & 0x8) {
                snprintf(op, DISASM_OP_SIZE, "MOVE");
                snprintf(args, DISASM_ARGS_SIZE, "USP, A%d", reg);
            } else {
                snprintf(op, DISASM_OP_SIZE, "MOVE");
                snprintf(args, DISASM_ARGS_SIZE, "A%d, USP", reg);
            }
        }

        else if ((opcode & 0xFFC0) == 0x4E80) {
            snprintf(op, DISASM_OP_SIZE, "JSR");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_LONG, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x4EC0) {
            snprintf(op, DISASM_OP_SIZE, "JMP");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_LONG, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x40C0) {
            snprintf(op, DISASM_OP_SIZE, "MOVE");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "SR, %s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x44C0) {
            snprintf(op, DISASM_OP_SIZE, "MOVE");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, CCR", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x46C0) {
            snprintf(op, DISASM_OP_SIZE, "MOVE");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, SR", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x4AC0) {
            snprintf(op, DISASM_OP_SIZE, "TAS");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_BYTE, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x4800) {
            snprintf(op, DISASM_OP_SIZE, "NBCD");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_BYTE, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x4840) {
            int mode = (opcode >> 3) & 7;
            if (mode == 0) {
                snprintf(op, DISASM_OP_SIZE, "SWAP");
                snprintf(args, DISASM_ARGS_SIZE, "D%d", opcode & 7);
            } else {
                snprintf(op, DISASM_OP_SIZE, "PEA");
                char ea_buf[DISASM_EA_SIZE];
                int ea_bytes = disasm_ea(cpu, pc + 2, mode, opcode & 7, SIZE_LONG, ea_buf);
                snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
                len += ea_bytes;
            }
        }

        else if ((opcode & 0xFB80) == 0x4880) {
            int mode = (opcode >> 3) & 7;
            if (mode == 0) {
                bool is_long = (opcode & 0x0040) != 0;
                snprintf(op, DISASM_OP_SIZE, "EXT%s", is_long ? ".L" : ".W");
                snprintf(args, DISASM_ARGS_SIZE, "D%d", opcode & 7);
            } else {
                bool is_long = (opcode & 0x0040) != 0;
                snprintf(op, DISASM_OP_SIZE, "MOVEM%s", is_long ? ".L" : ".W");

                len += 2;
                char ea_buf[DISASM_EA_SIZE];
                int ea_bytes = disasm_ea(cpu, pc + 4, mode, opcode & 7,
                                         is_long ? SIZE_LONG : SIZE_WORD, ea_buf);
                snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
                len += ea_bytes;
            }
        }

        else if ((opcode & 0xF1C0) == 0x4180) {
            snprintf(op, DISASM_OP_SIZE, "CHK");
            int reg = (opcode >> 9) & 7;
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, D%d", ea_buf, reg);
            len += ea_bytes;
        }

        else if ((opcode & 0xF1C0) == 0x41C0) {
            snprintf(op, DISASM_OP_SIZE, "LEA");
            int reg = (opcode >> 9) & 7;
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_LONG, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, A%d", ea_buf, reg);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4A00) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            snprintf(op, DISASM_OP_SIZE, "TST%s", size_str(size_code));
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4000) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            snprintf(op, DISASM_OP_SIZE, "NEGX%s", size_str(size_code));
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4200) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            snprintf(op, DISASM_OP_SIZE, "CLR%s", size_str(size_code));
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4400) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            snprintf(op, DISASM_OP_SIZE, "NEG%s", size_str(size_code));
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4600) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            snprintf(op, DISASM_OP_SIZE, "NOT%s", size_str(size_code));
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        } else {
            snprintf(op, DISASM_OP_SIZE, "MISC");
        }
    }

    else if ((opcode & 0xF000) == 0x5000) {
        if ((opcode & 0xC0) == 0xC0) {
            int cond = (opcode >> 8) & 0xF;
            static const char* cond_names[] = {"T",  "F",  "HI", "LS", "CC", "CS", "NE", "EQ",
                                               "VC", "VS", "PL", "MI", "GE", "LT", "GT", "LE"};
            int mode = (opcode >> 3) & 7;
            if (mode == 1) {
                snprintf(op, DISASM_OP_SIZE, "DB%s", cond_names[cond]);
                s16 disp = (s16)peek_word(cpu, pc + 2);
                len += 2;
                snprintf(args, DISASM_ARGS_SIZE, "D%d, $%X", opcode & 7, pc + 2 + disp);
            } else {
                snprintf(op, DISASM_OP_SIZE, "S%s", cond_names[cond]);
                char ea_buf[DISASM_EA_SIZE];
                int ea_bytes = disasm_ea(cpu, pc + 2, mode, opcode & 7, SIZE_BYTE, ea_buf);
                snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
                len += ea_bytes;
            }
        } else {
            bool sub = (opcode & 0x0100) != 0;
            int data = (opcode >> 9) & 7;
            if (data == 0) data = 8;
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            snprintf(op, DISASM_OP_SIZE, "%sQ%s", sub ? "SUB" : "ADD", size_str(size_code));
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "#%d, %s", data, ea_buf);
            len += ea_bytes;
        }
    }

    else if ((opcode & 0xF000) == 0x6000) {
        len = decode_branch(cpu, pc, opcode, op, args);
    }

    else if ((opcode & 0xF000) == 0x7000) {
        snprintf(op, DISASM_OP_SIZE, "MOVEQ");
        int reg = (opcode >> 9) & 7;
        s8 data = (s8)(opcode & 0xFF);
        snprintf(args, DISASM_ARGS_SIZE, "#%d, D%d", data, reg);
    }

    else if ((opcode & 0xF000) == 0x8000) {
        int opmode = (opcode >> 6) & 7;
        if (opmode == 3) {
            snprintf(op, DISASM_OP_SIZE, "DIVU.W");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, D%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if (opmode == 7) {
            snprintf(op, DISASM_OP_SIZE, "DIVS.W");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, D%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if ((opcode & 0x1F0) == 0x100) {
            snprintf(op, DISASM_OP_SIZE, "SBCD");
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            if (opcode & 8)
                snprintf(args, DISASM_ARGS_SIZE, "-(A%d), -(A%d)", ry, rx);
            else
                snprintf(args, DISASM_ARGS_SIZE, "D%d, D%d", ry, rx);
        } else {
            len = decode_std_arith(cpu, pc, opcode, op, args, "OR");
        }
    }

    else if ((opcode & 0xF000) == 0x9000) {
        int opmode = (opcode >> 6) & 7;
        if (opmode == 3 || opmode == 7) {
            snprintf(op, DISASM_OP_SIZE, "SUBA%s", opmode == 3 ? ".W" : ".L");
            char ea_buf[DISASM_EA_SIZE];
            int size = opmode == 3 ? SIZE_WORD : SIZE_LONG;
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, A%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if ((opcode & 0x0130) == 0x0100) {
            int size_code = (opcode >> 6) & 3;
            snprintf(op, DISASM_OP_SIZE, "SUBX%s", size_str(size_code));
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            if (opcode & 8)
                snprintf(args, DISASM_ARGS_SIZE, "-(A%d), -(A%d)", ry, rx);
            else
                snprintf(args, DISASM_ARGS_SIZE, "D%d, D%d", ry, rx);
        } else {
            len = decode_std_arith(cpu, pc, opcode, op, args, "SUB");
        }
    }

    else if ((opcode & 0xF000) == 0xB000) {
        int opmode = (opcode >> 6) & 7;
        int mode = (opcode >> 3) & 7;
        if (opmode == 3 || opmode == 7) {
            snprintf(op, DISASM_OP_SIZE, "CMPA%s", opmode == 3 ? ".W" : ".L");
            char ea_buf[DISASM_EA_SIZE];
            int size = opmode == 3 ? SIZE_WORD : SIZE_LONG;
            int ea_bytes = disasm_ea(cpu, pc + 2, mode, opcode & 7, size, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, A%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if (opmode >= 4 && mode == 1) {
            int size_code = opmode - 4;
            snprintf(op, DISASM_OP_SIZE, "CMPM%s", size_str(size_code));
            snprintf(args, DISASM_ARGS_SIZE, "(A%d)+, (A%d)+", opcode & 7, (opcode >> 9) & 7);
        } else if (opmode >= 4) {
            len = decode_std_arith(cpu, pc, opcode, op, args, "EOR");
        } else {
            len = decode_std_arith(cpu, pc, opcode, op, args, "CMP");
        }
    }

    else if ((opcode & 0xF000) == 0xC000) {
        int opmode = (opcode >> 6) & 7;
        if (opmode == 3) {
            snprintf(op, DISASM_OP_SIZE, "MULU.W");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, D%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if (opmode == 7) {
            snprintf(op, DISASM_OP_SIZE, "MULS.W");
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, D%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if ((opcode & 0x1F0) == 0x100) {
            snprintf(op, DISASM_OP_SIZE, "ABCD");
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            if (opcode & 8)
                snprintf(args, DISASM_ARGS_SIZE, "-(A%d), -(A%d)", ry, rx);
            else
                snprintf(args, DISASM_ARGS_SIZE, "D%d, D%d", ry, rx);
        } else if ((opcode & 0x0130) == 0x0100) {
            snprintf(op, DISASM_OP_SIZE, "EXG");
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            int exg_mode = (opcode >> 3) & 0x1F;
            if (exg_mode == 0x08)
                snprintf(args, DISASM_ARGS_SIZE, "D%d, D%d", rx, ry);
            else if (exg_mode == 0x09)
                snprintf(args, DISASM_ARGS_SIZE, "A%d, A%d", rx, ry);
            else
                snprintf(args, DISASM_ARGS_SIZE, "D%d, A%d", rx, ry);
        } else {
            len = decode_std_arith(cpu, pc, opcode, op, args, "AND");
        }
    }

    else if ((opcode & 0xF000) == 0xD000) {
        int opmode = (opcode >> 6) & 7;
        if (opmode == 3 || opmode == 7) {
            snprintf(op, DISASM_OP_SIZE, "ADDA%s", opmode == 3 ? ".W" : ".L");
            char ea_buf[DISASM_EA_SIZE];
            int size = opmode == 3 ? SIZE_WORD : SIZE_LONG;
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s, A%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if ((opcode & 0x0130) == 0x0100) {
            int size_code = (opcode >> 6) & 3;
            snprintf(op, DISASM_OP_SIZE, "ADDX%s", size_str(size_code));
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            if (opcode & 8)
                snprintf(args, DISASM_ARGS_SIZE, "-(A%d), -(A%d)", ry, rx);
            else
                snprintf(args, DISASM_ARGS_SIZE, "D%d, D%d", ry, rx);
        } else {
            len = decode_std_arith(cpu, pc, opcode, op, args, "ADD");
        }
    }

    else if ((opcode & 0xF000) == 0xE000) {
        if ((opcode & 0xC0) == 0xC0) {
            static const char* mem_ops[] = {"ASR",  "ASL",  "LSR", "LSL",
                                            "ROXR", "ROXL", "ROR", "ROL"};
            int type = (opcode >> 9) & 3;
            int dir = (opcode >> 8) & 1;
            snprintf(op, DISASM_OP_SIZE, "%s.W", mem_ops[type * 2 + dir]);
            char ea_buf[DISASM_EA_SIZE];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            snprintf(args, DISASM_ARGS_SIZE, "%s", ea_buf);
            len += ea_bytes;
        } else {
            static const char* reg_ops[] = {"AS", "LS", "ROX", "RO"};
            int type = (opcode >> 3) & 3;
            int dir = (opcode >> 8) & 1;
            int size_code = (opcode >> 6) & 3;
            int count_reg = (opcode >> 9) & 7;
            bool imm_count = ((opcode >> 5) & 1) == 0;

            snprintf(op, DISASM_OP_SIZE, "%s%c%s", reg_ops[type], dir ? 'L' : 'R', size_str(size_code));
            if (imm_count) {
                int count = count_reg == 0 ? 8 : count_reg;
                snprintf(args, DISASM_ARGS_SIZE, "#%d, D%d", count, opcode & 7);
            } else {
                snprintf(args, DISASM_ARGS_SIZE, "D%d, D%d", count_reg, opcode & 7);
            }
        }
    }

    snprintf_s(buffer, buf_size, "%04X  %-8s %s", opcode, op, args);
    return len;
}
