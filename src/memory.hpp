// memory.hpp - Memoria plana byte-addressable de 32 bits (little-endian)
//
// El espacio de direcciones es de 4 GiB, pero no reservamos 4 GiB de RAM:
// la memoria se divide en "paginas" de 4 KiB que solo se crean cuando
// alguien escribe en ellas. Leer una direccion nunca escrita devuelve 0.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

class Memory {
public:
    static constexpr uint32_t PAGE_BITS = 12;
    static constexpr uint32_t PAGE_SIZE = 1u << PAGE_BITS;  // 4096 bytes

    uint8_t read8(uint32_t addr) const {
        auto it = pages_.find(addr >> PAGE_BITS);
        if (it == pages_.end()) return 0;
        return (*it->second)[addr & (PAGE_SIZE - 1)];
    }

    void write8(uint32_t addr, uint8_t value) {
        auto& page = pages_[addr >> PAGE_BITS];
        if (!page) {
            page = std::make_unique<Page>();
            page->fill(0);
        }
        (*page)[addr & (PAGE_SIZE - 1)] = value;
    }

    // Little-endian: el byte menos significativo va en la direccion mas baja.
    uint16_t read16(uint32_t addr) const {
        return static_cast<uint16_t>(read8(addr) | (read8(addr + 1) << 8));
    }

    uint32_t read32(uint32_t addr) const {
        return static_cast<uint32_t>(read8(addr)) |
               (static_cast<uint32_t>(read8(addr + 1)) << 8) |
               (static_cast<uint32_t>(read8(addr + 2)) << 16) |
               (static_cast<uint32_t>(read8(addr + 3)) << 24);
    }

    void write16(uint32_t addr, uint16_t value) {
        write8(addr, value & 0xFF);
        write8(addr + 1, (value >> 8) & 0xFF);
    }

    void write32(uint32_t addr, uint32_t value) {
        write8(addr, value & 0xFF);
        write8(addr + 1, (value >> 8) & 0xFF);
        write8(addr + 2, (value >> 16) & 0xFF);
        write8(addr + 3, (value >> 24) & 0xFF);
    }

    void clear() { pages_.clear(); }

private:
    using Page = std::array<uint8_t, PAGE_SIZE>;
    std::unordered_map<uint32_t, std::unique_ptr<Page>> pages_;
};
