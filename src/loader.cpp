// loader.cpp - Cargar un programa (binario crudo o ELF32) a memoria
#include "loader.hpp"

#include <fstream>
#include <iterator>
#include <vector>

namespace rv {

static uint16_t rd16(const std::vector<uint8_t>& b, size_t o) {
    return static_cast<uint16_t>(b[o] | (b[o + 1] << 8));
}
static uint32_t rd32(const std::vector<uint8_t>& b, size_t o) {
    return static_cast<uint32_t>(b[o]) | (static_cast<uint32_t>(b[o + 1]) << 8) |
           (static_cast<uint32_t>(b[o + 2]) << 16) | (static_cast<uint32_t>(b[o + 3]) << 24);
}

static LoadInfo load_elf(const std::vector<uint8_t>& f, Memory& mem) {
    LoadInfo info;
    info.is_elf = true;

    if (f.size() < 52) { info.error = "ELF demasiado pequeno"; return info; }
    if (f[4] != 1) { info.error = "Solo se soporta ELF de 32 bits"; return info; }
    if (f[5] != 1) { info.error = "Solo se soporta ELF little-endian"; return info; }
    if (rd16(f, 18) != 0xF3) { info.error = "El ELF no es de RISC-V"; return info; }

    info.entry = rd32(f, 24);
    const uint32_t phoff = rd32(f, 28);
    const uint16_t phentsize = rd16(f, 42);
    const uint16_t phnum = rd16(f, 44);

    // Recorremos los "program headers" y copiamos los segmentos PT_LOAD
    for (uint16_t i = 0; i < phnum; ++i) {
        const size_t ph = phoff + static_cast<size_t>(i) * phentsize;
        if (ph + 32 > f.size()) { info.error = "Program header fuera del archivo"; return info; }
        if (rd32(f, ph) != 1) continue;  // 1 = PT_LOAD

        const uint32_t offset = rd32(f, ph + 4);
        const uint32_t paddr = rd32(f, ph + 12);
        const uint32_t filesz = rd32(f, ph + 16);
        const uint32_t memsz = rd32(f, ph + 20);
        if (static_cast<size_t>(offset) + filesz > f.size()) {
            info.error = "Segmento fuera del archivo";
            return info;
        }
        for (uint32_t k = 0; k < memsz; ++k)  // lo que sobra (.bss) se llena con 0
            mem.write8(paddr + k, k < filesz ? f[offset + k] : 0);
        info.size += memsz;
        if (paddr + memsz > info.end_addr) info.end_addr = paddr + memsz;
    }
    info.ok = true;
    return info;
}

LoadInfo load_program(const std::string& path, Memory& mem) {
    LoadInfo info;
    std::ifstream file(path, std::ios::binary);
    if (!file) { info.error = "No se pudo abrir \"" + path + "\""; return info; }

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

    if (bytes.size() >= 4 && bytes[0] == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F')
        return load_elf(bytes, mem);

    // Binario crudo: byte i del archivo -> direccion i de memoria
    for (size_t i = 0; i < bytes.size(); ++i) mem.write8(static_cast<uint32_t>(i), bytes[i]);
    info.ok = true;
    info.entry = 0;
    info.size = static_cast<uint32_t>(bytes.size());
    info.end_addr = info.size;
    return info;
}

}  // namespace rv
