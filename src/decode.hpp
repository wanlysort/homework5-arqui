// decode.hpp - Separar una instruccion de 32 bits en sus campos
//
// Todas las instrucciones RV32I miden 32 bits. Segun el "formato"
// (R, I, S, B, U, J) los bits significan cosas distintas. Aqui sacamos
// cada campo y armamos el inmediato con signo de cada formato.
#pragma once

#include <cstdint>

namespace rv {

// Opcodes del ISA base (bits [6:0])
enum Opcode : uint32_t {
    OP_LOAD   = 0x03,  // lb, lh, lw, lbu, lhu
    OP_MISC   = 0x0F,  // fence (se trata como nop)
    OP_IMM    = 0x13,  // addi, slti, ..., slli, srli, srai
    OP_AUIPC  = 0x17,
    OP_STORE  = 0x23,  // sb, sh, sw
    OP_REG    = 0x33,  // add, sub, ..., and
    OP_LUI    = 0x37,
    OP_BRANCH = 0x63,  // beq, bne, blt, bge, bltu, bgeu
    OP_JALR   = 0x67,
    OP_JAL    = 0x6F,
    OP_SYSTEM = 0x73,  // ecall, ebreak
};

// Extiende el signo de un valor de "bits" bits a 32 bits.
inline int32_t sign_extend(uint32_t value, int bits) {
    const uint32_t m = 1u << (bits - 1);
    value &= (bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1);
    return static_cast<int32_t>((value ^ m) - m);
}

struct Decoded {
    uint32_t raw;
    uint32_t opcode, rd, funct3, rs1, rs2, funct7;

    explicit Decoded(uint32_t inst)
        : raw(inst),
          opcode(inst & 0x7F),
          rd((inst >> 7) & 0x1F),
          funct3((inst >> 12) & 0x7),
          rs1((inst >> 15) & 0x1F),
          rs2((inst >> 20) & 0x1F),
          funct7((inst >> 25) & 0x7F) {}

    // Formato I: imm[11:0] = inst[31:20]
    int32_t imm_i() const { return sign_extend(raw >> 20, 12); }

    // Formato S: imm[11:5] = inst[31:25], imm[4:0] = inst[11:7]
    int32_t imm_s() const {
        return sign_extend(((raw >> 25) << 5) | ((raw >> 7) & 0x1F), 12);
    }

    // Formato B: imm[12|10:5|4:1|11], el bit 0 siempre es 0
    int32_t imm_b() const {
        uint32_t imm = (((raw >> 31) & 0x1) << 12) |
                       (((raw >> 7) & 0x1) << 11) |
                       (((raw >> 25) & 0x3F) << 5) |
                       (((raw >> 8) & 0xF) << 1);
        return sign_extend(imm, 13);
    }

    // Formato U: imm[31:12], los 12 bits bajos son 0
    int32_t imm_u() const { return static_cast<int32_t>(raw & 0xFFFFF000u); }

    // Formato J: imm[20|10:1|11|19:12], el bit 0 siempre es 0
    int32_t imm_j() const {
        uint32_t imm = (((raw >> 31) & 0x1) << 20) |
                       (((raw >> 12) & 0xFF) << 12) |
                       (((raw >> 20) & 0x1) << 11) |
                       (((raw >> 21) & 0x3FF) << 1);
        return sign_extend(imm, 21);
    }

    uint32_t shamt() const { return (raw >> 20) & 0x1F; }
};

}  // namespace rv
