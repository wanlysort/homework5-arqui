// disasm.cpp - Desensamblador sencillo de RV32I
#include "disasm.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

#include "decode.hpp"

namespace rv {

static const char* const ABI[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

const char* reg_name(uint32_t r) { return ABI[r & 31]; }

int parse_reg(const std::string& raw) {
    std::string s = raw;
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (s == "pc") return 32;
    if (s == "fp") return 8;
    if (s.size() >= 2 && s[0] == 'x' &&
        std::all_of(s.begin() + 1, s.end(), [](unsigned char c) { return std::isdigit(c); })) {
        int n = std::stoi(s.substr(1));
        return (n >= 0 && n < 32) ? n : -1;
    }
    for (int i = 0; i < 32; ++i)
        if (s == ABI[i]) return i;
    return -1;
}

static std::string hex(uint32_t v) {
    std::ostringstream s;
    s << "0x" << std::hex << v;
    return s.str();
}

std::string disassemble(uint32_t inst, uint32_t pc) {
    const Decoded d(inst);
    const std::string rd = reg_name(d.rd), rs1 = reg_name(d.rs1), rs2 = reg_name(d.rs2);
    std::ostringstream o;

    switch (d.opcode) {
        case OP_LUI:
            o << "lui " << rd << ", " << hex(static_cast<uint32_t>(d.imm_u()) >> 12);
            break;
        case OP_AUIPC:
            o << "auipc " << rd << ", " << hex(static_cast<uint32_t>(d.imm_u()) >> 12);
            break;
        case OP_JAL: {
            const uint32_t target = pc + d.imm_j();
            if (d.rd == 0) o << "j " << hex(target);
            else if (d.rd == 1) o << "jal " << hex(target);
            else o << "jal " << rd << ", " << hex(target);
            break;
        }
        case OP_JALR:
            if (d.rd == 0 && d.rs1 == 1 && d.imm_i() == 0) o << "ret";
            else o << "jalr " << rd << ", " << d.imm_i() << "(" << rs1 << ")";
            break;
        case OP_BRANCH: {
            static const char* names[8] = {"beq", "bne", "?", "?", "blt", "bge", "bltu", "bgeu"};
            o << names[d.funct3] << " " << rs1 << ", " << rs2 << ", " << hex(pc + d.imm_b());
            break;
        }
        case OP_LOAD: {
            static const char* names[8] = {"lb", "lh", "lw", "?", "lbu", "lhu", "?", "?"};
            o << names[d.funct3] << " " << rd << ", " << d.imm_i() << "(" << rs1 << ")";
            break;
        }
        case OP_STORE: {
            static const char* names[8] = {"sb", "sh", "sw", "?", "?", "?", "?", "?"};
            o << names[d.funct3] << " " << rs2 << ", " << d.imm_s() << "(" << rs1 << ")";
            break;
        }
        case OP_IMM: {
            if (inst == 0x00000013) { o << "nop"; break; }
            if (d.funct3 == 0 && d.rs1 == 0) { o << "li " << rd << ", " << d.imm_i(); break; }
            if (d.funct3 == 0 && d.imm_i() == 0) { o << "mv " << rd << ", " << rs1; break; }
            if (d.funct3 == 1) { o << "slli " << rd << ", " << rs1 << ", " << d.shamt(); break; }
            if (d.funct3 == 5) {
                o << (d.funct7 == 0x20 ? "srai " : "srli ") << rd << ", " << rs1 << ", " << d.shamt();
                break;
            }
            static const char* names[8] = {"addi", "", "slti", "sltiu", "xori", "", "ori", "andi"};
            o << names[d.funct3] << " " << rd << ", " << rs1 << ", " << d.imm_i();
            break;
        }
        case OP_REG: {
            static const char* names[8] = {"add", "sll", "slt", "sltu", "xor", "srl", "or", "and"};
            std::string name = names[d.funct3];
            if (d.funct7 == 0x20 && d.funct3 == 0) name = "sub";
            else if (d.funct7 == 0x20 && d.funct3 == 5) name = "sra";
            else if (d.funct7 != 0x00) name = "?";
            o << name << " " << rd << ", " << rs1 << ", " << rs2;
            break;
        }
        case OP_MISC:
            o << "fence";
            break;
        case OP_SYSTEM:
            if (inst == 0x00000073) o << "ecall";
            else if (inst == 0x00100073) o << "ebreak";
            else o << "???";
            break;
        default:
            o << "??? (" << hex(inst) << ")";
    }
    return o.str();
}

}  // namespace rv
