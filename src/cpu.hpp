// cpu.hpp - Estado arquitectural de una CPU RV32I y ejecucion de instrucciones
#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

#include "memory.hpp"

namespace rv {

// Resultado de ejecutar una instruccion
enum class StepResult {
    Ok,          // se ejecuto normalmente
    Halted,      // el programa termino (ecall exit o bucle infinito "j .")
    Breakpoint,  // se encontro una instruccion ebreak
    Error,       // instruccion invalida, PC desalineado, etc.
};

class CPU {
public:
    std::array<uint32_t, 32> x{};  // x0..x31 (x0 siempre vale 0)
    uint32_t pc = 0;
    Memory mem;

    uint64_t instret = 0;      // cuantas instrucciones se ejecutaron
    int exit_code = 0;         // codigo de salida (ecall exit)
    std::string last_error;    // mensaje del ultimo error

    // Entrada/salida de las ecall (se puede redirigir)
    std::istream* in = &std::cin;
    std::ostream* out = &std::cout;

    void reset(uint32_t entry = 0) {
        x.fill(0);
        pc = entry;
        instret = 0;
        exit_code = 0;
        last_error.clear();
    }

    // Ejecuta UNA instruccion: fetch -> decode -> execute -> actualizar estado
    StepResult step();

private:
    StepResult ecall();
    StepResult fail(const std::string& msg);
};

}  // namespace rv
