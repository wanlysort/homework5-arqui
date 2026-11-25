// disasm.hpp - Desensamblador sencillo de RV32I
#pragma once

#include <cstdint>
#include <string>

namespace rv {


const char* reg_name(uint32_t r);
int parse_reg(const std::string& name);
std::string disassemble(uint32_t inst, uint32_t pc);

}
