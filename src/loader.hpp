// loader.hpp - Cargar un programa (binario crudo o ELF32) a memoria
#pragma once

#include <cstdint>
#include <string>

#include "memory.hpp"

namespace rv {

struct LoadInfo {
    bool ok = false;
    bool is_elf = false;
    uint32_t entry = 0;      // donde empieza el PC
    uint32_t size = 0;       // bytes cargados
    uint32_t end_addr = 0;   // ultima direccion cargada + 1
    std::string error;
};

// Si el archivo empieza con la "firma" ELF (0x7F 'E' 'L' 'F') se cargan sus
// segmentos; si no, se copia tal cual (binario crudo) a partir de 0x00000000.
LoadInfo load_program(const std::string& path, Memory& mem);

}  // namespace rv
