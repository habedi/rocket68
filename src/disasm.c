#include "disasm.h"

#include <stdio.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// Helper to read memory without side effects (peeking)
static u16 peek_word(M68kCpu* cpu, u32 pc) {
    return m68k_read_16(cpu, pc);
}

static u32 peek_long(M68kCpu* cpu, u32 pc) {
    return m68k_read_32(cpu, pc);
}

// Size string helper
static const char* size_str(int size_code) {
    switch (size_code) {
        case 0: return ".B"; // 00
        case 1: return ".W"; // 01
        case 2: return ".L"; // 10
        case 3: return ".?"; // 11 (Invalid for most)
        default: return ".?";
    }
}

// -----------------------------------------------------------------------------
// Effective Address Decoding
// -----------------------------------------------------------------------------

// Returns number of bytes consumed by EA extensions
static int disasm_ea(M68kCpu* cpu, u32 base_pc, int mode, int reg, int size, char* buf) {
    int bytes = 0;
    
    switch (mode) {
        case 0: // Dn
            sprintf(buf, "D%d", reg);
            break;
        case 1: // An
            sprintf(buf, "A%d", reg);
            break;
        case 2: // (An)
            sprintf(buf, "(A%d)", reg);
            break;
        case 3: // (An)+
            sprintf(buf, "(A%d)+", reg);
            break;
        case 4: // -(An)
            sprintf(buf, "-(A%d)", reg);
            break;
        case 5: // d16(An)
        {
            s16 disp = (s16)peek_word(cpu, base_pc);
            sprintf(buf, "%d(A%d)", disp, reg);
            bytes = 2;
            break;
        }
        case 6: // d8(An,Xn)
        {
            u16 ext = peek_word(cpu, base_pc);
            s8 disp = (s8)(ext & 0xFF);
            int idx_reg = (ext >> 12) & 0x7;
            bool is_a_reg = (ext >> 15) & 1;
            bool is_long = (ext >> 11) & 1;
            sprintf(buf, "%d(A%d,%c%d.%c)", disp, reg, 
                    is_a_reg ? 'A' : 'D', idx_reg, is_long ? 'L' : 'W');
            bytes = 2;
            break;
        }
        case 7: // Misc
        {
            switch (reg) {
                case 0: // Abs.W
                {
                    u16 addr = peek_word(cpu, base_pc);
                    sprintf(buf, "$%04X.W", addr);
                    bytes = 2;
                    break;
                }
                case 1: // Abs.L
                {
                    u32 addr = peek_long(cpu, base_pc);
                    sprintf(buf, "$%08X.L", addr);
                    bytes = 4;
                    break;
                }
                case 2: // d16(PC)
                {
                    s16 disp = (s16)peek_word(cpu, base_pc);
                    sprintf(buf, "$%X(PC)", base_pc + disp); // Show effective address? Or disp?
                    // Let's show displacement for now, maybe effective label later
                    // sprintf(buf, "%d(PC)", disp);
                    bytes = 2;
                    break;
                }
                case 3: // d8(PC,Xn)
                {
                    u16 ext = peek_word(cpu, base_pc);
                    s8 disp = (s8)(ext & 0xFF);
                    int idx_reg = (ext >> 12) & 0x7;
                    bool is_a_reg = (ext >> 15) & 1;
                    bool is_long = (ext >> 11) & 1;
                    sprintf(buf, "%d(PC,%c%d.%c)", disp, 
                            is_a_reg ? 'A' : 'D', idx_reg, is_long ? 'L' : 'W');
                    bytes = 2;
                    break;
                }
                case 4: // Immediate
                {
                    if (size == SIZE_LONG) {
                        u32 val = peek_long(cpu, base_pc);
                        sprintf(buf, "#$%X", val);
                        bytes = 4;
                    } else if (size == SIZE_BYTE) {
                         u16 val = peek_word(cpu, base_pc);
                         sprintf(buf, "#$%02X", val & 0xFF);
                         bytes = 2;
                    } else { // Word
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

// -----------------------------------------------------------------------------
// Instruction Decoders
// -----------------------------------------------------------------------------

// MOVE
static int decode_move(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args) {
    int bytes = 2;
    int size_code = (opcode >> 12) & 0x3;
    int size;
    const char* sz_str;
    
    switch (size_code) { // Note: Move uses different size encoding
        case 1: size = SIZE_BYTE; sz_str = ".B"; break;
        case 3: size = SIZE_WORD; sz_str = ".W"; break;
        case 2: size = SIZE_LONG; sz_str = ".L"; break;
        default: return 0; // Invalid
    }
    
    sprintf(op, "MOVE%s", sz_str);
    
    // MOVE has Dest then Src in opcode, but we print Src, Dest
    int dest_reg = (opcode >> 9) & 0x7;
    int dest_mode = (opcode >> 6) & 0x7;
    int src_mode = (opcode >> 3) & 0x7;
    int src_reg = opcode & 0x7;
    
    // Src EA
    char src_buf[32];
    int src_bytes = disasm_ea(cpu, pc + bytes, src_mode, src_reg, size, src_buf);
    bytes += src_bytes;
    
    // Dest EA
    char dest_buf[32];
    int dest_bytes = disasm_ea(cpu, pc + bytes, dest_mode, dest_reg, size, dest_buf);
    bytes += dest_bytes;
    
    sprintf(args, "%s, %s", src_buf, dest_buf);
    return bytes;
}

// BRA / Bcc
static int decode_branch(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args) {
    int cond = (opcode >> 8) & 0xF;
    s8 d8 = (s8)(opcode & 0xFF);
    int bytes = 2;
    
    const char* cc_names[] = {
        "RA", "SR", "HI", "LS", "CC", "CS", "NE", "EQ",
        "VC", "VS", "PL", "MI", "GE", "LT", "GT", "LE"
    };
    
    if (cond == 1) sprintf(op, "BSR"); // BSR
    else if (cond == 0) sprintf(op, "BRA"); // BRA
    else sprintf(op, "B%s", cc_names[cond]);
    
    if (d8 == 0) {
        // 16-bit disp
        s16 d16 = (s16)peek_word(cpu, pc + 2);
        sprintf(args, "$%X", pc + 2 + d16);
        bytes += 2;
    } else if (d8 == -1) {
        // 32-bit disp (68020+) - Not implemented, treat as 8 bit -1?
        // Actually 68000 treats FF as 8 bit disp -1.
        sprintf(args, "$%X", pc + 2 + d8);
    } else {
        sprintf(args, "$%X", pc + 2 + d8);
    }
    
    return bytes;
}

// ADD/SUB/AND/OR/EOR/CMP
// Standard format: Op <Rx> <OpMode> <Ea>
// OpMode defines mapping.
static int decode_std_arith(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args, const char* name_base) {
    int reg = (opcode >> 9) & 0x7;
    int opmode = (opcode >> 6) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int ea_reg = opcode & 0x7;
    int bytes = 2;
    int size = SIZE_WORD;
    const char* sz = ".W";
    
    // Opmodes:
    // 000: Byte, Dn <op> Ea -> Dn
    // 001: Word, Dn <op> Ea -> Dn
    // 010: Long, Dn <op> Ea -> Dn
    // 100: Byte, Ea <op> Dn -> Ea
    // 101: Word, Ea <op> Dn -> Ea
    // 110: Long, Ea <op> Dn -> Ea
    
    bool dn_dest = true;
    
    switch (opmode) {
        case 0: size = SIZE_BYTE; sz = ".B"; break;
        case 1: size = SIZE_WORD; sz = ".W"; break;
        case 2: size = SIZE_LONG; sz = ".L"; break;
        case 4: size = SIZE_BYTE; sz = ".B"; dn_dest = false; break;
        case 5: size = SIZE_WORD; sz = ".W"; dn_dest = false; break;
        case 6: size = SIZE_LONG; sz = ".L"; dn_dest = false; break;
        default: return 0; // Maybe ADDA/SUBA/CMPA logic handled elsewhere or fallback
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


// Immediates (ADDI, subi, etc)
static int decode_imm_arith(M68kCpu* cpu, u32 pc, u16 opcode, char* op, char* args, const char* name) {
    int bytes = 2;
    int size_code = (opcode >> 6) & 0x3;
    int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
    
    sprintf(op, "%s%s", name, size_str(size_code));
    
    // Immediate data
    char imm_buf[32];
    int imm_bytes = disasm_ea(cpu, pc + bytes, 7, 4, size, imm_buf);
    bytes += imm_bytes;
    
    // Dest EA
    char ea_buf[32];
    int ea_mode = (opcode >> 3) & 0x7;
    int ea_reg = opcode & 0x7;
    int ea_bytes = disasm_ea(cpu, pc + bytes, ea_mode, ea_reg, size, ea_buf);
    bytes += ea_bytes;
    
    sprintf(args, "%s, %s", imm_buf, ea_buf);
    return bytes;
}

// -----------------------------------------------------------------------------
// Main Disassembler Interface
// -----------------------------------------------------------------------------

int m68k_disasm(M68kCpu* cpu, u32 pc, char* buffer, int buf_size) {
    u16 opcode = m68k_read_16(cpu, pc);
    char op[32] = "???";
    char args[64] = "";
    int len = 2;

    // Decoding Tree
    
    // 0000 ....
    if ((opcode & 0xF000) == 0x0000) {
        // Bit ops, MoveP, ORI, ANDI, SUBI, ADDI, EORI, CMP2... complex group
        if ((opcode & 0x0100) == 0x0100) {
             // 0000 ... 1 ... -> Probably BTST/BCHG dynamic or MOVEP
             sprintf(op, "BIT/MOVEP");
        } else if ((opcode & 0x0800) == 0) {
             // 0000 0... -> ORI / ANDI / SUBI / ADDI / EORI... wait, 
             // ORI: 0000 000 0 SS ...
             // ANDI: 0000 001 0 SS ...
             // SUBI: 0000 010 0 SS ...
             // ADDI: 0000 011 0 SS ...
             // EORI: 0000 101 0 SS ...
             // CMPI: 0000 110 0 SS ...
             // BTST static: 0000 100 0 SS ...
             int type = (opcode >> 8) & 0xF;
             switch (type) {
                 case 0: len = decode_imm_arith(cpu, pc, opcode, op, args, "ORI"); break;
                 case 2: len = decode_imm_arith(cpu, pc, opcode, op, args, "ANDI"); break;
                 case 4: len = decode_imm_arith(cpu, pc, opcode, op, args, "SUBI"); break;
                 case 6: len = decode_imm_arith(cpu, pc, opcode, op, args, "ADDI"); break;
                 case 10: len = decode_imm_arith(cpu, pc, opcode, op, args, "EORI"); break;
                 case 12: len = decode_imm_arith(cpu, pc, opcode, op, args, "CMPI"); break;
                 default: sprintf(op, "IMM-GRP0"); break;
             }
        } else {
             sprintf(op, "GRP0-UNK");
        }
    }
    // 00xx (Move)
    if ((opcode & 0xC000) == 0x0000 && (opcode & 0xF000) != 0) { // 00XX where top bits 00 is Move, but ruled out 0000 group
         // Wait, Move is 00ZZ ...
         // Move: 00 SS ...
         // If SS is not 00, it's a Move?
         // No, 0000group is specific.
         // Let's rely on decode_move to check 'invalid' if it was Group 0?
         // Actually, Move instructions have 00 at top. 
         // If bits 12-13 are non-zero?
         // No, Move.B is 00 01 ... (0x1...)
         // Move.L is 00 10 ... (0x2...)
         // Move.W is 00 11 ... (0x3...)
         // So 0x1000 - 0x3FFF are Moves.
         len = decode_move(cpu, pc, opcode, op, args);
    }
    
    // 0100 (Misc)
    else if ((opcode & 0xF000) == 0x4000) {
        if ((opcode & 0xF1C0) == 0x4180) { // CHK
             sprintf(op, "CHK");
             int reg = (opcode >> 9) & 0x7;
             char ea_buf[32];
             int ea_len = disasm_ea(cpu, pc+2, (opcode>>3)&7, opcode&7, SIZE_WORD, ea_buf);
             sprintf(args, "%s, D%d", ea_buf, reg);
             len += ea_len;
        } else if ((opcode & 0xF1C0) == 0x41C0) { // LEA
             sprintf(op, "LEA");
             int reg = (opcode >> 9) & 0x7;
             char ea_buf[32];
             int ea_len = disasm_ea(cpu, pc+2, (opcode>>3)&7, opcode&7, SIZE_LONG, ea_buf);
             sprintf(args, "%s, A%d", ea_buf, reg);
             len += ea_len;
        } else if (opcode == 0x4E75) sprintf(op, "RTS");
        else if (opcode == 0x4E73) sprintf(op, "RTE");
        else if (opcode == 0x4E70) sprintf(op, "RESET");
        else if (opcode == 0x4E71) sprintf(op, "NOP");
        else if (opcode == 0x4E72) { sprintf(op, "STOP"); sprintf(args, "#$%X", peek_word(cpu, pc+2)); len+=2; }
        else if ((opcode & 0xFFF0) == 0x4E40) { sprintf(op, "TRAP"); sprintf(args, "#%d", opcode & 0xF); }
        else if ((opcode & 0xFFC0) == 0x4800) { 
            sprintf(op, "NBCD"); 
            char ea_buf[32];
            int ea_len = disasm_ea(cpu, pc+2, (opcode>>3)&7, opcode&7, SIZE_BYTE, ea_buf);
            sprintf(args, "%s", ea_buf);
            len += ea_len;
        } 
        else if ((opcode & 0xFF00) == 0x4A00) { 
             sprintf(op, "TST"); 
             int size = (opcode >> 6) & 3;
             size = (size == 0) ? SIZE_BYTE : (size == 1 ? SIZE_WORD : SIZE_LONG);
             const char* sz = (size == SIZE_BYTE) ? ".B" : ((size == SIZE_WORD) ? ".W" : ".L");
             strcat(op, sz);
             char ea_buf[32];
             int ea_len = disasm_ea(cpu, pc+2, (opcode>>3)&7, opcode&7, size, ea_buf);
             sprintf(args, "%s", ea_buf);
             len += ea_len;
        }
        else sprintf(op, "MISC");
    }
    
    // 0101 (ADDQ/SUBQ/Scc/DBcc)
    else if ((opcode & 0xF000) == 0x5000) {
        if ((opcode & 0xC0) == 0xC0) { // Scc / DBcc
             if ((opcode & 0x8) == 0x8) sprintf(op, "DBcc"); // ...
             else sprintf(op, "Scc");
        } else {
             // ADDQ/SUBQ
             bool sub = (opcode & 0x0100);
             int data = (opcode >> 9) & 7;
             if (data == 0) data = 8;
             int size_code = (opcode >> 6) & 3;
             int size = (size_code == 0) ? SIZE_BYTE : (size_code == 1 ? SIZE_WORD : SIZE_LONG);
             const char* sz = (size_code == 0) ? ".B" : ((size_code == 1) ? ".W" : ".L");
             
             sprintf(op, "%sQ%s", sub ? "SUB" : "ADD", sz);
             
             char ea_buf[32];
             int ea_len = disasm_ea(cpu, pc+2, (opcode>>3)&7, opcode&7, size, ea_buf);
             sprintf(args, "#%d, %s", data, ea_buf);
             len += ea_len;
        }
    }
    
    // 0110 (Branch)
    else if ((opcode & 0xF000) == 0x6000) {
        len = decode_branch(cpu, pc, opcode, op, args);
    }
    
    // 0111 (MOVEQ)
    else if ((opcode & 0xF000) == 0x7000) {
        sprintf(op, "MOVEQ");
        int reg = (opcode >> 9) & 7;
        s8 data = (s8)(opcode & 0xFF);
        sprintf(args, "#%d, D%d", data, reg);
    }
    
    // 1000 (OR / DIV / SBCD)
    else if ((opcode & 0xF000) == 0x8000) {
        if (((opcode >> 6) & 7) == 3 || ((opcode >> 6) & 7) == 7) {
             sprintf(op, "DIVU/S");
        } else if ((opcode & 0x1F0) == 0x100) {
             sprintf(op, "SBCD");
        } else {
             len = decode_std_arith(cpu, pc, opcode, op, args, "OR");
        }
    }
    
    // 1001 (SUB / SUBX)
    else if ((opcode & 0xF000) == 0x9000) {
         if ((opcode & 0xC0) == 0xC0) sprintf(op, "SUBA"); // Simplification
         else if ((opcode & 0x0130) == 0x0100) sprintf(op, "SUBX");
         else len = decode_std_arith(cpu, pc, opcode, op, args, "SUB");
    }
    
    // 1011 (CMP, EOR)
    else if ((opcode & 0xF000) == 0xB000) {
        if ((opcode & 0x00C0) == 0xC0) sprintf(op, "CMPA");
        else if ((opcode & 0x0100)) len = decode_std_arith(cpu, pc, opcode, op, args, "EOR"); // sort of
        else len = decode_std_arith(cpu, pc, opcode, op, args, "CMP");
    }
    
    // 1100 (AND / MUL / ABCD / EXG)
    else if ((opcode & 0xF000) == 0xC000) {
        if ((opcode & 0x1F0) == 0x100) sprintf(op, "ABCD");
        else if ((opcode & 0x130) == 0x100) sprintf(op, "EXG");
        else if (((opcode >> 6) & 7) == 3 || ((opcode >> 6) & 7) == 7) sprintf(op, "MULU/S");
        else len = decode_std_arith(cpu, pc, opcode, op, args, "AND");
    }
    
    // 1101 (ADD / ADDX)
    else if ((opcode & 0xF000) == 0xD000) {
         if ((opcode & 0xC0) == 0xC0) sprintf(op, "ADDA");
         else if ((opcode & 0x0130) == 0x0100) sprintf(op, "ADDX");
         else len = decode_std_arith(cpu, pc, opcode, op, args, "ADD");
    }

    snprintf(buffer, buf_size, "%04X  %-8s %s", opcode, op, args);
    return len;
}
