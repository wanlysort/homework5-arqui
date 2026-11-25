
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "cpu.hpp"
#include "disasm.hpp"
#include "loader.hpp"

using namespace rv;

namespace {

constexpr uint64_t RUN_LIMIT = 50'000'000;  // tope 

std::string hex8(uint32_t v) {
    std::ostringstream s;
    s << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << v;
    return s.str();
}

bool parse_num(const std::string& s, uint32_t& out) {
    try {
        size_t used = 0;
        unsigned long long v = std::stoull(s, &used, 0);  // base 0: acepta 0x.., decimal
        if (used != s.size() || v > 0xFFFFFFFFull) return false;
        out = static_cast<uint32_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> split(const std::string& line) {
    std::istringstream ss(line);
    std::vector<std::string> t;
    std::string w;
    while (ss >> w) t.push_back(w);
    return t;
}

class Simulator {
public:
    bool load(const std::string& path) {
        cpu_.mem.clear();
        LoadInfo info = load_program(path, cpu_.mem);
        if (!info.ok) {
            std::cout << "Error: " << info.error << "\n";
            loaded_ = false;
            return false;
        }
        path_ = path;
        info_ = info;
        loaded_ = true;
        finished_ = false;
        cpu_.reset(info.entry);
        std::cout << "\"" << path << "\" cargado a memoria (" << info.size << " bytes, "
                  << (info.is_elf ? "ELF" : "binario crudo") << ", inicio en "
                  << hex8(info.entry) << ").\n";
        return true;
    }

    // Ejecuta un comando. Devuelve false si hay que salir.
    bool command(const std::string& line) {
        auto t = split(line);
        if (t.empty()) return true;
        const std::string& c = t[0];

        if (c == "exit" || c == "quit" || c == "q") return false;
        if (c == "help" || c == "h" || c == "?") { help(); return true; }
        if (c == "load") {
            if (t.size() < 2) std::cout << "Uso: load <archivo>\n";
            else load(t[1]);
            return true;
        }
        if (!loaded_) { std::cout << "No hay programa cargado. Usa: load <archivo>\n"; return true; }

        if (c == "step" || c == "s") cmd_step(t);
        else if (c == "run" || c == "continue" || c == "c") cmd_run();
        else if (c == "pc") std::cout << "pc = " << hex8(cpu_.pc) << "\n";
        else if (c == "regs" || c == "r") cmd_regs(t);
        else if (c == "mem" || c == "m") cmd_mem(t);
        else if (c == "memw" || c == "mw") cmd_memw(t);
        else if (c == "disasm" || c == "d") cmd_disasm(t);
        else if (c == "break" || c == "b") cmd_break(t);
        else if (c == "delete") cmd_delete(t);
        else if (c == "reset") load(path_);
        else if (c == "info") cmd_info();
        else std::cout << "Comando desconocido: \"" << c << "\". Escribe 'help'.\n";
        return true;
    }

    int exit_code() const { return cpu_.exit_code; }

private:
    CPU cpu_;
    LoadInfo info_;
    std::string path_;
    bool loaded_ = false;
    bool finished_ = false;
    std::set<uint32_t> breakpoints_;

    // Ejecuta una instruccion mostrando que hizo. Devuelve false si hay que parar.
    bool do_step(bool verbose) {
        if (finished_) {
            std::cout << "El programa ya termino. Usa 'reset' para reiniciarlo.\n";
            return false;
        }
        const uint32_t pc = cpu_.pc;
        const uint32_t inst = cpu_.mem.read32(pc);
        const auto before = cpu_.x;

        if (verbose)
            std::cout << hex8(pc) << ":  " << std::hex << std::setw(8) << std::setfill('0')
                      << inst << std::dec << std::setfill(' ') << "   " << disassemble(inst, pc) << "\n";

        StepResult r = cpu_.step();

        if (verbose) {  // mostrar que registros cambiaron
            for (int i = 1; i < 32; ++i)
                if (cpu_.x[i] != before[i])
                    std::cout << "            " << std::left << std::setw(4) << reg_name(i)
                              << std::right << " (x" << i << "): " << hex8(before[i])
                              << " -> " << hex8(cpu_.x[i]) << "\n";
        }
        return report(r);
    }

    bool report(StepResult r) {
        switch (r) {
            case StepResult::Ok: return true;
            case StepResult::Halted:
                finished_ = true;
                std::cout << "\nPrograma terminado:" << hex8(cpu_.pc) << " despues de "
                          << cpu_.instret << " instrucciones, codigo " << cpu_.exit_code << "\n";
                return false;
            case StepResult::Breakpoint:
                std::cout << "ebreak en " << hex8(cpu_.pc - 4) << "\n";
                return false;
            case StepResult::Error:
                finished_ = true;
                std::cout << "error" << cpu_.last_error << "\n";
                return false;
        }
        return false;
    }

    void cmd_step(const std::vector<std::string>& t) {
        uint32_t n = 1;
        if (t.size() > 1 && !parse_num(t[1], n)) { std::cout << "Uso de step n\n"; return; }
        for (uint32_t i = 0; i < n; ++i)
            if (!do_step(true)) break;
    }

    void cmd_run() {
        if (finished_) { do_step(false); return; }
        uint64_t count = 0;
        bool first = true;
        while (count < RUN_LIMIT) {
            if (!first && breakpoints_.count(cpu_.pc)) {
                std::cout << "Breakpoint en " << hex8(cpu_.pc) << "  "
                          << disassemble(cpu_.mem.read32(cpu_.pc), cpu_.pc) << "\n";
                return;
            }
            first = false;
            if (!report(cpu_.step())) return;
            ++count;
        }
        std::cout << "Se detuvo tras " << RUN_LIMIT << " instrucciones (limite)\n";
    }

    void print_reg(int i) {
        std::cout << std::left << std::setw(5) << ("x" + std::to_string(i)) << "("
                  << std::setw(4) << reg_name(i) << ") = " << std::right << hex8(cpu_.x[i])
                  << "  (" << static_cast<int32_t>(cpu_.x[i]) << ")";
    }

    void cmd_regs(const std::vector<std::string>& t) {
        if (t.size() == 1) {  // todos los registros en 2 columnas
            std::cout << "pc = " << hex8(cpu_.pc) << "\n";
            for (int i = 0; i < 16; ++i) {
                std::cout << std::left << std::setw(4) << ("x" + std::to_string(i))
                          << std::setw(5) << reg_name(i) << std::right << hex8(cpu_.x[i])
                          << "        " << std::left << std::setw(4)
                          << ("x" + std::to_string(i + 16)) << std::setw(5) << reg_name(i + 16)
                          << std::right << hex8(cpu_.x[i + 16]) << "\n";
            }
            return;
        }
        for (size_t k = 1; k < t.size(); ++k) {
            int r = parse_reg(t[k]);
            if (r < 0) std::cout << "Registro desconocido " << t[k] << "\n";
            else if (r == 32) std::cout << "pc = " << hex8(cpu_.pc) << "\n";
            else { print_reg(r); std::cout << "\n"; }
        }
    }

    // mem <inicio> [fin]  -> bytes (como en el enunciado)
    void cmd_mem(const std::vector<std::string>& t) {
        uint32_t a, b;
        if (t.size() < 2 || !parse_num(t[1], a)) { std::cout << "Uso: mem <inicio> [fin]\n"; return; }
        if (t.size() < 3) b = a + 15;
        else if (!parse_num(t[2], b) || b < a) { std::cout << "Rango invalido\n"; return; }
        if (b - a > 4095) b = a + 4095;

        std::cout << std::uppercase << std::hex << std::setfill('0');
        if (b - a < 16) {
            std::cout << "Memoria (" << hex8(a) << "-" << hex8(b) << "):";
            for (uint32_t p = a;; ++p) { std::cout << " " << std::setw(2) << int(cpu_.mem.read8(p)); if (p == b) break; }
            std::cout << "\n";
        } else {
            for (uint32_t row = a; row <= b; row += 16) {
                std::cout << hex8(row) << ":";
                for (uint32_t p = row; p < row + 16 && p <= b; ++p)
                    std::cout << " " << std::setw(2) << int(cpu_.mem.read8(p));
                std::cout << "\n";
                if (row + 16 < row) break;  // desborde
            }
        }
        std::cout << std::dec << std::setfill(' ') << std::nouppercase;
    }

    // memw <inicio> [cantidad]  -> palabras de 32 bits con su valor decimal
    void cmd_memw(const std::vector<std::string>& t) {
        uint32_t a, n = 8;
        if (t.size() < 2 || !parse_num(t[1], a) || (t.size() > 2 && !parse_num(t[2], n))) {
            std::cout << "Uso: memw <inicio> [cantidad]\n";
            return;
        }
        for (uint32_t i = 0; i < n && i < 1024; ++i) {
            uint32_t addr = a + 4 * i, v = cpu_.mem.read32(addr);
            std::cout << hex8(addr) << ":  " << hex8(v) << "  (" << static_cast<int32_t>(v) << ")\n";
        }
    }

    // disasm [inicio] [cantidad]
    void cmd_disasm(const std::vector<std::string>& t) {
        uint32_t a = cpu_.pc, n = 10;
        if ((t.size() > 1 && !parse_num(t[1], a)) || (t.size() > 2 && !parse_num(t[2], n))) {
            std::cout << "Uso: disasm [inicio] [cantidad]\n";
            return;
        }
        a &= ~3u;
        for (uint32_t i = 0; i < n && i < 1024; ++i) {
            uint32_t addr = a + 4 * i, inst = cpu_.mem.read32(addr);
            std::cout << (addr == cpu_.pc ? "=> " : "   ") << (breakpoints_.count(addr) ? "*" : " ")
                      << hex8(addr) << ":  " << std::hex << std::setw(8) << std::setfill('0') << inst
                      << std::dec << std::setfill(' ') << "   " << disassemble(inst, addr) << "\n";
        }
    }

    void cmd_break(const std::vector<std::string>& t) {
        uint32_t a;
        if (t.size() < 2) {
            if (breakpoints_.empty()) std::cout << "No hay breakpoints.\n";
            for (uint32_t b : breakpoints_) std::cout << "  " << hex8(b) << "\n";
            return;
        }
        if (!parse_num(t[1], a)) { std::cout << "Uso: break <direccion>\n"; return; }
        breakpoints_.insert(a);
        std::cout << "Breakpoint en " << hex8(a) << "\n";
    }

    void cmd_delete(const std::vector<std::string>& t) {
        uint32_t a;
        if (t.size() < 2) { breakpoints_.clear(); std::cout << "Breakpoints borrados.\n"; return; }
        if (!parse_num(t[1], a)) { std::cout << "Uso: delete [direccion]\n"; return; }
        breakpoints_.erase(a);
    }

    void cmd_info() {
        std::cout << "Archivo: " << path_ << "\nFormato: " << (info_.is_elf ? "ELF" : "binario crudo")
                  << "\nPC: " << hex8(cpu_.pc) << "\nInstrucciones ejecutadas: " << cpu_.instret
                  << "\nEstado: " << (finished_ ? "terminado" : "listo") << "\n";
    }

    void help() {
        std::cout <<
            "Comandos:\n"
            "  step [n]            (s)  ejecuta n instrucciones (por defecto 1)\n"
            "  run                 (c)  ejecuta hasta terminar, ebreak o breakpoint\n"
            "  pc                       muestra el contador de programa\n"
            "  regs [r1 r2 ...]    (r)  muestra todos los registros o solo algunos (x5, t0, sp...)\n"
            "  mem <ini> [fin]     (m)  muestra bytes de memoria\n"
            "  memw <ini> [n]      (mw) muestra n palabras de 32 bits\n"
            "  disasm [ini] [n]    (d)  desensambla n instrucciones (por defecto desde el pc)\n"
            "  break [dir]         (b)  pone un breakpoint (sin argumento: lista)\n"
            "  delete [dir]             borra un breakpoint (sin argumento: todos)\n"
            "  reset                    vuelve a cargar el programa y reinicia la CPU\n"
            "  load <archivo>           carga otro programa (.bin crudo o .elf)\n"
            "  info                     informacion del programa cargado\n"
            "  exit                (q)  salir\n";
    }
};

}  // namespace

int main(int argc, char** argv) {
    std::cout << " Simulador RISC-V RV32I   \n";

    Simulator sim;
    std::string script;
    bool run_all = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--run") run_all = true;
        else if (arg == "--script" && i + 1 < argc) script = argv[++i];
        else sim.load(arg);
    }

    if (run_all) {
        sim.command("run");
        sim.command("regs");
        return sim.exit_code();
    }

    std::ifstream script_file;
    std::istream* input = &std::cin;
    if (!script.empty()) {
        script_file.open(script);
        if (!script_file) { std::cout << "No se pudo abrir  " << script << "\n"; return 1; }
        input = &script_file;
    }

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(*input, line)) break;
        if (input != &std::cin) std::cout << line << "\n";  // eco del script
        if (!sim.command(line)) break;
    }
    std::cout << "Program exited with code " << sim.exit_code() << ".\n";
    return sim.exit_code();
}
