// cpu.cpp - Ejecucion de las 37 instrucciones RV32I del anexo A (+ ecall/ebreak/fence)
#include "cpu.hpp"

#include <iomanip>
#include <sstream>

#include "decode.hpp"

namespace rv {

StepResult CPU::fail(const std::string& msg) {
    std::ostringstream ss;
    ss << msg << " (pc = 0x" << std::hex << std::setw(8) << std::setfill('0')
       << pc << ")";
    last_error = ss.str();
    return StepResult::Error;
}

StepResult CPU::step() {
    // ---------- 1. FETCH: leer la instruccion apuntada por el PC ----------
    if (pc % 4 != 0) return fail("PC desalineado");
    const uint32_t inst = mem.read32(pc);

    // ---------- 2. DECODE: separar los campos ----------
    const Decoded d(inst);
    const uint32_t a = x[d.rs1];          // valor de rs1
    const uint32_t b = x[d.rs2];          // valor de rs2
    const int32_t sa = static_cast<int32_t>(a);
    const int32_t sb = static_cast<int32_t>(b);

    uint32_t next_pc = pc + 4;            // por defecto, la siguiente instruccion
    bool write_rd = false;                // la instruccion escribe en rd?
    uint32_t result = 0;                  // valor que se escribe en rd
    bool is_jump = false;                 // para detectar "j ." (bucle infinito)

    // ---------- 3. EXECUTE ----------
    switch (d.opcode) {
        case OP_LUI:
            result = static_cast<uint32_t>(d.imm_u());
            write_rd = true;
            break;

        case OP_AUIPC:
            result = pc + static_cast<uint32_t>(d.imm_u());
            write_rd = true;
            break;

        case OP_JAL:
            result = pc + 4;  // direccion de retorno
            write_rd = true;
            next_pc = pc + static_cast<uint32_t>(d.imm_j());
            is_jump = true;
            break;

        case OP_JALR:
            if (d.funct3 != 0) return fail("jalr invalido");
            result = pc + 4;
            write_rd = true;
            next_pc = (a + static_cast<uint32_t>(d.imm_i())) & ~1u;
            is_jump = true;
            break;

        case OP_BRANCH: {
            bool taken;
            switch (d.funct3) {
                case 0: taken = (a == b); break;    // beq
                case 1: taken = (a != b); break;    // bne
                case 4: taken = (sa < sb); break;   // blt
                case 5: taken = (sa >= sb); break;  // bge
                case 6: taken = (a < b); break;     // bltu
                case 7: taken = (a >= b); break;    // bgeu
                default: return fail("Branch invalido");
            }
            if (taken) next_pc = pc + static_cast<uint32_t>(d.imm_b());
            is_jump = true;
            break;
        }

        case OP_LOAD: {
            const uint32_t addr = a + static_cast<uint32_t>(d.imm_i());
            switch (d.funct3) {
                case 0: result = static_cast<uint32_t>(sign_extend(mem.read8(addr), 8)); break;    // lb
                case 1: result = static_cast<uint32_t>(sign_extend(mem.read16(addr), 16)); break;  // lh
                case 2: result = mem.read32(addr); break;                                          // lw
                case 4: result = mem.read8(addr); break;                                           // lbu
                case 5: result = mem.read16(addr); break;                                          // lhu
                default: return fail("Load invalido");
            }
            write_rd = true;
            break;
        }

        case OP_STORE: {
            const uint32_t addr = a + static_cast<uint32_t>(d.imm_s());
            switch (d.funct3) {
                case 0: mem.write8(addr, b & 0xFF); break;      // sb
                case 1: mem.write16(addr, b & 0xFFFF); break;   // sh
                case 2: mem.write32(addr, b); break;            // sw
                default: return fail("Store invalido");
            }
            break;
        }

        case OP_IMM: {
            const int32_t imm = d.imm_i();
            const uint32_t uimm = static_cast<uint32_t>(imm);
            switch (d.funct3) {
                case 0: result = a + uimm; break;                              // addi
                case 2: result = (sa < imm) ? 1 : 0; break;                    // slti
                case 3: result = (a < uimm) ? 1 : 0; break;                    // sltiu
                case 4: result = a ^ uimm; break;                              // xori
                case 6: result = a | uimm; break;                              // ori
                case 7: result = a & uimm; break;                              // andi
                case 1:                                                        // slli
                    if (d.funct7 != 0x00) return fail("slli invalido");
                    result = a << d.shamt();
                    break;
                case 5:
                    if (d.funct7 == 0x00)      result = a >> d.shamt();                              // srli
                    else if (d.funct7 == 0x20) result = static_cast<uint32_t>(sa >> d.shamt());      // srai
                    else return fail("Shift inmediato invalido");
                    break;
            }
            write_rd = true;
            break;
        }

        case OP_REG: {
            const uint32_t sh = b & 0x1F;  // solo importan los 5 bits bajos
            if (d.funct7 == 0x00) {
                switch (d.funct3) {
                    case 0: result = a + b; break;                       // add
                    case 1: result = a << sh; break;                     // sll
                    case 2: result = (sa < sb) ? 1 : 0; break;           // slt
                    case 3: result = (a < b) ? 1 : 0; break;             // sltu
                    case 4: result = a ^ b; break;                       // xor
                    case 5: result = a >> sh; break;                     // srl
                    case 6: result = a | b; break;                       // or
                    case 7: result = a & b; break;                       // and
                }
            } else if (d.funct7 == 0x20 && d.funct3 == 0) {
                result = a - b;                                          // sub
            } else if (d.funct7 == 0x20 && d.funct3 == 5) {
                result = static_cast<uint32_t>(sa >> sh);                // sra
            } else {
                std::ostringstream s;
                s << "Instruccion R invalida 0x" << std::hex << inst;
                return fail(s.str());
            }
            write_rd = true;
            break;
        }

        case OP_MISC:  // fence: no hace nada en un simulador de un solo hilo
            break;

        case OP_SYSTEM:
            if (inst == 0x00000073) {  // ecall
                StepResult r = ecall();
                if (r != StepResult::Ok) return r;
            } else if (inst == 0x00100073) {  // ebreak
                pc += 4;
                ++instret;
                return StepResult::Breakpoint;
            } else {
                return fail("Instruccion SYSTEM no soportada");
            }
            break;

        default: {
            std::ostringstream s;
            s << "Instruccion invalida 0x" << std::hex << std::setw(8)
              << std::setfill('0') << inst;
            return fail(s.str());
        }
    }

    // ---------- 4. ACTUALIZAR ESTADO ----------
    if (write_rd && d.rd != 0) x[d.rd] = result;
    x[0] = 0;  // x0 siempre es cero

    ++instret;
    const uint32_t old_pc = pc;
    pc = next_pc;

    
    if (is_jump && next_pc == old_pc) return StepResult::Halted;
    return StepResult::Ok;
}

// Llamadas al entorno estilo SPIM / CPUlator. El numero de servicio va en a7.
StepResult CPU::ecall() {
    uint32_t& a0 = x[10];
    const uint32_t a1 = x[11];
    const uint32_t a7 = x[17];

    switch (a7) {
        case 1:  // print_int
            *out << static_cast<int32_t>(a0) << std::flush;
            break;
        case 4: {  // print_string (termina en '\0')
            for (uint32_t p = a0;; ++p) {
                char c = static_cast<char>(mem.read8(p));
                if (c == '\0') break;
                *out << c;
            }
            *out << std::flush;
            break;
        }
        case 5: {  // read_int
            int32_t v = 0;
            *in >> v;
            a0 = static_cast<uint32_t>(v);
            break;
        }
        case 8: {  // read_string(a0 = buffer, a1 = tamano maximo)
            std::string line;
            std::getline(*in >> std::ws, line);
            uint32_t n = 0;
            for (; a1 > 0 && n < a1 - 1 && n < line.size(); ++n) mem.write8(a0 + n, line[n]);
            if (a1 > 0) mem.write8(a0 + n, 0);
            break;
        }
        case 10:  // exit
            exit_code = 0;
            ++instret;
            pc += 4;
            return StepResult::Halted;
        case 11:  // print_char
            *out << static_cast<char>(a0 & 0xFF) << std::flush;
            break;
        case 12: {  // read_char
            char c = 0;
            in->get(c);
            a0 = static_cast<uint8_t>(c);
            break;
        }
        case 17:  // exit2 (codigo en a0)
        case 93:  // exit (convencion Linux)
            exit_code = static_cast<int32_t>(a0);
            ++instret;
            pc += 4;
            return StepResult::Halted;
        case 34: {  // print_int_hex
            std::ios f(nullptr);
            f.copyfmt(*out);
            *out << "0x" << std::hex << std::setw(8) << std::setfill('0') << a0 << std::flush;
            out->copyfmt(f);
            break;
        }
        case 36:  // print_int_unsigned
            *out << a0 << std::flush;
            break;
        default: {
            std::ostringstream s;
            s << "ecall no soportada (a7 = " << a7 << ")";
            return fail(s.str());
        }
    }
    return StepResult::Ok;
}

}  // namespace rv
