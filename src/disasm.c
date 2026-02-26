#if defined(__STDC_LIB_EXT1__)
#define __STDC_WANT_LIB_EXT1__ 1
#endif

#include "disasm.h"

#include <stdio.h>
#include <string.h>

#ifndef __STDC_LIB_EXT1__
#define snprintf_s(buf, sz, fmt, ...) snprintf(buf, sz, fmt, ##__VA_ARGS__)
#endif

static u16 peek_word(M68kCpu* cpu, u32 pc) { return m68k_read_16(cpu, pc); }

static u32 peek_long(M68kCpu* cpu, u32 pc) { return m68k_read_32(cpu, pc); }

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
            sprintf(buf, "D%d", reg);
            break;
        case 1:
            sprintf(buf, "A%d", reg);
            break;
        case 2:
            sprintf(buf, "(A%d)", reg);
            break;
        case 3:
            sprintf(buf, "(A%d)+", reg);
            break;
        case 4:
            sprintf(buf, "-(A%d)", reg);
            break;
        case 5: {
            s16 disp = (s16)peek_word(cpu, base_pc);
            sprintf(buf, "%d(A%d)", disp, reg);
            bytes = 2;
            break;
        }
        case 6: {
            u16 ext = peek_word(cpu, base_pc);
            s8 disp = (s8)(ext & 0xFF);
            int idx_reg = (ext >> 12) & 0x7;
            bool is_a_reg = (ext >> 15) & 1;
            bool is_long = (ext >> 11) & 1;
            sprintf(buf, "%d(A%d,%c%d.%c)", disp, reg, is_a_reg ? 'A' : 'D', idx_reg,
                    is_long ? 'L' : 'W');
            bytes = 2;
            break;
        }
        case 7: {
            switch (reg) {
                case 0: {
                    u16 addr = peek_word(cpu, base_pc);
                    sprintf(buf, "$%04X.W", addr);
                    bytes = 2;
                    break;
                }
                case 1: {
                    u32 addr = peek_long(cpu, base_pc);
                    sprintf(buf, "$%08X.L", addr);
                    bytes = 4;
                    break;
                }
                case 2: {
                    s16 disp = (s16)peek_word(cpu, base_pc);
                    sprintf(buf, "$%X(PC)", base_pc + disp);

                    bytes = 2;
                    break;
                }
                case 3: {
                    u16 ext = peek_word(cpu, base_pc);
                    s8 disp = (s8)(ext & 0xFF);
                    int idx_reg = (ext >> 12) & 0x7;
                    bool is_a_reg = (ext >> 15) & 1;
                    bool is_long = (ext >> 11) & 1;
                    sprintf(buf, "%d(PC,%c%d.%c)", disp, is_a_reg ? 'A' : 'D', idx_reg,
                            is_long ? 'L' : 'W');
                    bytes = 2;
                    break;
                }
                case 4: {
                    if (size == SIZE_LONG) {
                        u32 val = peek_long(cpu, base_pc);
                        sprintf(buf, "#$%X", val);
                        bytes = 4;
                    } else if (size == SIZE_BYTE) {
                        u16 val = peek_word(cpu, base_pc);
                        sprintf(buf, "#$%02X", val & 0xFF);
                        bytes = 2;
                    } else {
                        u16 val = peek_word(cpu, base_pc);
                        sprintf(buf, "#$%04X", val);
                        bytes = 2;
                    }
                    break;
                }
                default:
                    sprintf(buf, "???");
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

    sprintf(op, "MOVE%s", sz_str);

    int dest_reg = (opcode >> 9) & 0x7;
    int dest_mode = (opcode >> 6) & 0x7;
    int src_mode = (opcode >> 3) & 0x7;
    int src_reg = opcode & 0x7;

    char src_buf[32];
    int src_bytes = disasm_ea(cpu, pc + bytes, src_mode, src_reg, size, src_buf);
    bytes += src_bytes;

    char dest_buf[32];
    int dest_bytes = disasm_ea(cpu, pc + bytes, dest_mode, dest_reg, size, dest_buf);
    bytes += dest_bytes;

    sprintf(args, "%s, %s", src_buf, dest_buf);
    return bytes;
}

static int decode_branch(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args) {
    int cond = (opcode >> 8) & 0xF;
    s8 d8 = (s8)(opcode & 0xFF);
    int bytes = 2;

    const char* cc_names[] = {"RA", "SR", "HI", "LS", "CC", "CS", "NE", "EQ",
                              "VC", "VS", "PL", "MI", "GE", "LT", "GT", "LE"};

    if (cond == 1)
        sprintf(op, "BSR");
    else if (cond == 0)
        sprintf(op, "BRA");
    else
        sprintf(op, "B%s", cc_names[cond]);

    if (d8 == 0) {
        s16 d16 = (s16)peek_word(cpu, pc + 2);
        sprintf(args, "$%X", pc + 2 + d16);
        bytes += 2;
    } else if (d8 == -1) {
        sprintf(args, "$%X", pc + 2 + d8);
    } else {
        sprintf(args, "$%X", pc + 2 + d8);
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

    sprintf(op, "%s%s", name_base, sz);

    char ea_buf[32];
    int ea_bytes = disasm_ea(cpu, pc + bytes, mode, ea_reg, size, ea_buf);
    bytes += ea_bytes;

    if (dn_dest) {
        sprintf(args, "%s, D%d", ea_buf, reg);
    } else {
        sprintf(args, "D%d, %s", reg, ea_buf);
    }

    return bytes;
}

static int decode_imm_arith(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args,
                            const char* name) {
    int bytes = 2;
    int size_code = (opcode >> 6) & 0x3;
    int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);

    sprintf(op, "%s%s", name, size_str(size_code));

    char imm_buf[32];
    int imm_bytes = disasm_ea(cpu, pc + bytes, 7, 4, size, imm_buf);
    bytes += imm_bytes;

    char ea_buf[32];
    int ea_mode = (opcode >> 3) & 0x7;
    int ea_reg = opcode & 0x7;
    int ea_bytes = disasm_ea(cpu, pc + bytes, ea_mode, ea_reg, size, ea_buf);
    bytes += ea_bytes;

    sprintf(args, "%s, %s", imm_buf, ea_buf);
    return bytes;
}

int m68k_disasm(M68kCpu* cpu, u32 pc, char* buffer, int buf_size) {
    u16 opcode = m68k_read_16(cpu, pc);
    char op[32] = "???";
    char args[64] = "";
    int len = 2;

    if ((opcode & 0xF000) == 0x0000) {
        int top4 = (opcode >> 8) & 0xF;

        if (top4 == 0x08) {
            static const char* bit_names[] = {"BTST", "BCHG", "BCLR", "BSET"};
            int subop = (opcode >> 6) & 0x3;
            sprintf(op, "%s", bit_names[subop]);

            u16 bit_num = peek_word(cpu, pc + 2);
            len += 2;
            char ea_buf[32];
            int ea_bytes =
                disasm_ea(cpu, pc + len, (opcode >> 3) & 7, opcode & 7, SIZE_BYTE, ea_buf);
            len += ea_bytes;
            sprintf(args, "#%d, %s", bit_num & 0xFF, ea_buf);
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
                    sprintf(op, "MOVEP%s", (opmode == 5) ? ".L" : ".W");
                    sprintf(args, "%d(A%d), D%d", disp, areg, dn);
                } else {
                    sprintf(op, "MOVEP%s", (opmode == 7) ? ".L" : ".W");
                    sprintf(args, "D%d, %d(A%d)", dn, disp, areg);
                }
            } else {
                static const char* bit_names[] = {"BTST", "BCHG", "BCLR", "BSET"};
                int subop = (opcode >> 6) & 0x3;
                sprintf(op, "%s", bit_names[subop]);
                char ea_buf[32];
                int ea_bytes = disasm_ea(cpu, pc + 2, mode, opcode & 7, SIZE_BYTE, ea_buf);
                len += ea_bytes;
                sprintf(args, "D%d, %s", dn, ea_buf);
            }
        }

        else if (opcode == 0x003C) {
            sprintf(op, "ORI");
            sprintf(args, "#$%02X, CCR", peek_word(cpu, pc + 2) & 0xFF);
            len += 2;
        } else if (opcode == 0x007C) {
            sprintf(op, "ORI");
            sprintf(args, "#$%04X, SR", peek_word(cpu, pc + 2));
            len += 2;
        } else if (opcode == 0x023C) {
            sprintf(op, "ANDI");
            sprintf(args, "#$%02X, CCR", peek_word(cpu, pc + 2) & 0xFF);
            len += 2;
        } else if (opcode == 0x027C) {
            sprintf(op, "ANDI");
            sprintf(args, "#$%04X, SR", peek_word(cpu, pc + 2));
            len += 2;
        } else if (opcode == 0x0A3C) {
            sprintf(op, "EORI");
            sprintf(args, "#$%02X, CCR", peek_word(cpu, pc + 2) & 0xFF);
            len += 2;
        } else if (opcode == 0x0A7C) {
            sprintf(op, "EORI");
            sprintf(args, "#$%04X, SR", peek_word(cpu, pc + 2));
            len += 2;
        }

        else if ((opcode & 0x0800) == 0) {
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
            sprintf(op, "RESET");
        } else if (opcode == 0x4E71) {
            sprintf(op, "NOP");
        } else if (opcode == 0x4E72) {
            sprintf(op, "STOP");
            sprintf(args, "#$%04X", peek_word(cpu, pc + 2));
            len += 2;
        } else if (opcode == 0x4E73) {
            sprintf(op, "RTE");
        } else if (opcode == 0x4E75) {
            sprintf(op, "RTS");
        } else if (opcode == 0x4E76) {
            sprintf(op, "TRAPV");
        } else if (opcode == 0x4E77) {
            sprintf(op, "RTR");
        }

        else if ((opcode & 0xFFF0) == 0x4E40) {
            sprintf(op, "TRAP");
            sprintf(args, "#%d", opcode & 0xF);
        }

        else if ((opcode & 0xFFF8) == 0x4E48) {
            sprintf(op, "BKPT");
            sprintf(args, "#%d", opcode & 7);
        }

        else if ((opcode & 0xFFF8) == 0x4E50) {
            sprintf(op, "LINK");
            s16 disp = (s16)peek_word(cpu, pc + 2);
            len += 2;
            sprintf(args, "A%d, #%d", opcode & 7, disp);
        }

        else if ((opcode & 0xFFF8) == 0x4E58) {
            sprintf(op, "UNLK");
            sprintf(args, "A%d", opcode & 7);
        }

        else if ((opcode & 0xFFF0) == 0x4E60) {
            int reg = opcode & 7;
            if (opcode & 0x8) {
                sprintf(op, "MOVE");
                sprintf(args, "USP, A%d", reg);
            } else {
                sprintf(op, "MOVE");
                sprintf(args, "A%d, USP", reg);
            }
        }

        else if ((opcode & 0xFFC0) == 0x4E80) {
            sprintf(op, "JSR");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_LONG, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x4EC0) {
            sprintf(op, "JMP");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_LONG, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x40C0) {
            sprintf(op, "MOVE");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            sprintf(args, "SR, %s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x44C0) {
            sprintf(op, "MOVE");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            sprintf(args, "%s, CCR", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x46C0) {
            sprintf(op, "MOVE");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            sprintf(args, "%s, SR", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x4AC0) {
            sprintf(op, "TAS");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_BYTE, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x4800) {
            sprintf(op, "NBCD");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_BYTE, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFFC0) == 0x4840) {
            int mode = (opcode >> 3) & 7;
            if (mode == 0) {
                sprintf(op, "SWAP");
                sprintf(args, "D%d", opcode & 7);
            } else {
                sprintf(op, "PEA");
                char ea_buf[32];
                int ea_bytes = disasm_ea(cpu, pc + 2, mode, opcode & 7, SIZE_LONG, ea_buf);
                sprintf(args, "%s", ea_buf);
                len += ea_bytes;
            }
        }

        else if ((opcode & 0xFB80) == 0x4880) {
            int mode = (opcode >> 3) & 7;
            if (mode == 0) {
                bool is_long = (opcode & 0x0040) != 0;
                sprintf(op, "EXT%s", is_long ? ".L" : ".W");
                sprintf(args, "D%d", opcode & 7);
            } else {
                bool is_long = (opcode & 0x0040) != 0;
                sprintf(op, "MOVEM%s", is_long ? ".L" : ".W");

                len += 2;
                char ea_buf[32];
                int ea_bytes = disasm_ea(cpu, pc + 4, mode, opcode & 7,
                                         is_long ? SIZE_LONG : SIZE_WORD, ea_buf);
                sprintf(args, "%s", ea_buf);
                len += ea_bytes;
            }
        }

        else if ((opcode & 0xF1C0) == 0x4180) {
            sprintf(op, "CHK");
            int reg = (opcode >> 9) & 7;
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            sprintf(args, "%s, D%d", ea_buf, reg);
            len += ea_bytes;
        }

        else if ((opcode & 0xF1C0) == 0x41C0) {
            sprintf(op, "LEA");
            int reg = (opcode >> 9) & 7;
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_LONG, ea_buf);
            sprintf(args, "%s, A%d", ea_buf, reg);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4A00) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            sprintf(op, "TST%s", size_str(size_code));
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4000) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            sprintf(op, "NEGX%s", size_str(size_code));
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4200) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            sprintf(op, "CLR%s", size_str(size_code));
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4400) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            sprintf(op, "NEG%s", size_str(size_code));
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        }

        else if ((opcode & 0xFF00) == 0x4600) {
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            sprintf(op, "NOT%s", size_str(size_code));
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        } else {
            sprintf(op, "MISC");
        }
    }

    else if ((opcode & 0xF000) == 0x5000) {
        if ((opcode & 0xC0) == 0xC0) {
            int cond = (opcode >> 8) & 0xF;
            static const char* cond_names[] = {"T",  "F",  "HI", "LS", "CC", "CS", "NE", "EQ",
                                               "VC", "VS", "PL", "MI", "GE", "LT", "GT", "LE"};
            int mode = (opcode >> 3) & 7;
            if (mode == 1) {
                sprintf(op, "DB%s", cond_names[cond]);
                s16 disp = (s16)peek_word(cpu, pc + 2);
                len += 2;
                sprintf(args, "D%d, $%X", opcode & 7, pc + 2 + disp);
            } else {
                sprintf(op, "S%s", cond_names[cond]);
                char ea_buf[32];
                int ea_bytes = disasm_ea(cpu, pc + 2, mode, opcode & 7, SIZE_BYTE, ea_buf);
                sprintf(args, "%s", ea_buf);
                len += ea_bytes;
            }
        } else {
            bool sub = (opcode & 0x0100) != 0;
            int data = (opcode >> 9) & 7;
            if (data == 0) data = 8;
            int size_code = (opcode >> 6) & 3;
            int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
            sprintf(op, "%sQ%s", sub ? "SUB" : "ADD", size_str(size_code));
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            sprintf(args, "#%d, %s", data, ea_buf);
            len += ea_bytes;
        }
    }

    else if ((opcode & 0xF000) == 0x6000) {
        len = decode_branch(cpu, pc, opcode, op, args);
    }

    else if ((opcode & 0xF000) == 0x7000) {
        sprintf(op, "MOVEQ");
        int reg = (opcode >> 9) & 7;
        s8 data = (s8)(opcode & 0xFF);
        sprintf(args, "#%d, D%d", data, reg);
    }

    else if ((opcode & 0xF000) == 0x8000) {
        int opmode = (opcode >> 6) & 7;
        if (opmode == 3) {
            sprintf(op, "DIVU.W");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            sprintf(args, "%s, D%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if (opmode == 7) {
            sprintf(op, "DIVS.W");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            sprintf(args, "%s, D%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if ((opcode & 0x1F0) == 0x100) {
            sprintf(op, "SBCD");
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            if (opcode & 8)
                sprintf(args, "-(A%d), -(A%d)", ry, rx);
            else
                sprintf(args, "D%d, D%d", ry, rx);
        } else {
            len = decode_std_arith(cpu, pc, opcode, op, args, "OR");
        }
    }

    else if ((opcode & 0xF000) == 0x9000) {
        int opmode = (opcode >> 6) & 7;
        if (opmode == 3 || opmode == 7) {
            sprintf(op, "SUBA%s", opmode == 3 ? ".W" : ".L");
            char ea_buf[32];
            int size = opmode == 3 ? SIZE_WORD : SIZE_LONG;
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            sprintf(args, "%s, A%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if ((opcode & 0x0130) == 0x0100) {
            int size_code = (opcode >> 6) & 3;
            sprintf(op, "SUBX%s", size_str(size_code));
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            if (opcode & 8)
                sprintf(args, "-(A%d), -(A%d)", ry, rx);
            else
                sprintf(args, "D%d, D%d", ry, rx);
        } else {
            len = decode_std_arith(cpu, pc, opcode, op, args, "SUB");
        }
    }

    else if ((opcode & 0xF000) == 0xB000) {
        int opmode = (opcode >> 6) & 7;
        int mode = (opcode >> 3) & 7;
        if (opmode == 3 || opmode == 7) {
            sprintf(op, "CMPA%s", opmode == 3 ? ".W" : ".L");
            char ea_buf[32];
            int size = opmode == 3 ? SIZE_WORD : SIZE_LONG;
            int ea_bytes = disasm_ea(cpu, pc + 2, mode, opcode & 7, size, ea_buf);
            sprintf(args, "%s, A%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if (opmode >= 4 && mode == 1) {
            int size_code = opmode - 4;
            sprintf(op, "CMPM%s", size_str(size_code));
            sprintf(args, "(A%d)+, (A%d)+", opcode & 7, (opcode >> 9) & 7);
        } else if (opmode >= 4) {
            len = decode_std_arith(cpu, pc, opcode, op, args, "EOR");
        } else {
            len = decode_std_arith(cpu, pc, opcode, op, args, "CMP");
        }
    }

    else if ((opcode & 0xF000) == 0xC000) {
        int opmode = (opcode >> 6) & 7;
        if (opmode == 3) {
            sprintf(op, "MULU.W");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            sprintf(args, "%s, D%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if (opmode == 7) {
            sprintf(op, "MULS.W");
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            sprintf(args, "%s, D%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if ((opcode & 0x1F0) == 0x100) {
            sprintf(op, "ABCD");
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            if (opcode & 8)
                sprintf(args, "-(A%d), -(A%d)", ry, rx);
            else
                sprintf(args, "D%d, D%d", ry, rx);
        } else if ((opcode & 0x0130) == 0x0100) {
            sprintf(op, "EXG");
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            int exg_mode = (opcode >> 3) & 0x1F;
            if (exg_mode == 0x08)
                sprintf(args, "D%d, D%d", rx, ry);
            else if (exg_mode == 0x09)
                sprintf(args, "A%d, A%d", rx, ry);
            else
                sprintf(args, "D%d, A%d", rx, ry);
        } else {
            len = decode_std_arith(cpu, pc, opcode, op, args, "AND");
        }
    }

    else if ((opcode & 0xF000) == 0xD000) {
        int opmode = (opcode >> 6) & 7;
        if (opmode == 3 || opmode == 7) {
            sprintf(op, "ADDA%s", opmode == 3 ? ".W" : ".L");
            char ea_buf[32];
            int size = opmode == 3 ? SIZE_WORD : SIZE_LONG;
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, size, ea_buf);
            sprintf(args, "%s, A%d", ea_buf, (opcode >> 9) & 7);
            len += ea_bytes;
        } else if ((opcode & 0x0130) == 0x0100) {
            int size_code = (opcode >> 6) & 3;
            sprintf(op, "ADDX%s", size_str(size_code));
            int rx = (opcode >> 9) & 7, ry = opcode & 7;
            if (opcode & 8)
                sprintf(args, "-(A%d), -(A%d)", ry, rx);
            else
                sprintf(args, "D%d, D%d", ry, rx);
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
            sprintf(op, "%s.W", mem_ops[type * 2 + dir]);
            char ea_buf[32];
            int ea_bytes = disasm_ea(cpu, pc + 2, (opcode >> 3) & 7, opcode & 7, SIZE_WORD, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_bytes;
        } else {
            static const char* reg_ops[] = {"AS", "LS", "ROX", "RO"};
            int type = (opcode >> 3) & 3;
            int dir = (opcode >> 8) & 1;
            int size_code = (opcode >> 6) & 3;
            int count_reg = (opcode >> 9) & 7;
            bool imm_count = ((opcode >> 5) & 1) == 0;

            sprintf(op, "%s%c%s", reg_ops[type], dir ? 'L' : 'R', size_str(size_code));
            if (imm_count) {
                int count = count_reg == 0 ? 8 : count_reg;
                sprintf(args, "#%d, D%d", count, opcode & 7);
            } else {
                sprintf(args, "D%d, D%d", count_reg, opcode & 7);
            }
        }
    }

    snprintf_s(buffer, buf_size, "%04X  %-8s %s", opcode, op, args);
    return len;
}
