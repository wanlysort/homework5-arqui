# rvsim — Simulador del ISA RISC-V (RV32I)

CS3051 Arquitectura de Computadores 2026-II — Tarea 5

Simulador de consola escrito en **C++17** que carga un programa RISC-V ya compilado
(binario crudo o `.elf`), lo ejecuta instrucción por instrucción y permite ver el PC,
los 32 registros y la memoria en cualquier momento.

## Compilar

Con g++ (Linux, macOS o Windows con MinGW):

```sh
g++ -std=c++17 -O2 -o rvsim src/*.cpp
```

Con CMake (también sirve con Visual Studio):

```sh
cmake -B build
cmake --build build --config Release
```

## Usar

```sh
./rvsim programs/quicksort.bin                     # modo interactivo
./rvsim programs/quicksort.bin --run               # corre hasta el final y muestra registros
./rvsim programs/quicksort.bin --script cmds.txt   # lee los comandos desde un archivo
```

| Comando | Atajo | Qué hace |
|---|---|---|
| `step [n]` | `s` | Ejecuta n instrucciones (1 por defecto) y muestra qué registros cambiaron |
| `run` | `c` | Ejecuta hasta que el programa termine, un `ebreak` o un breakpoint |
| `pc` | | Muestra el contador de programa |
| `regs [r ...]` | `r` | Todos los registros, o solo los pedidos (`x5`, `t0`, `sp`, `a0`...) |
| `mem <ini> [fin]` | `m` | Muestra bytes de memoria (como en el enunciado) |
| `memw <ini> [n]` | `mw` | Muestra n palabras de 32 bits con su valor decimal |
| `disasm [ini] [n]` | `d` | Desensambla instrucciones (por defecto desde el PC) |
| `break [dir]` / `delete [dir]` | `b` | Pone / quita breakpoints |
| `reset` | | Vuelve a cargar el programa y reinicia la CPU |
| `load <archivo>` | | Carga otro programa |
| `info`, `help`, `exit` | | |

Ejemplo:

```text
$ ./rvsim programs/riscvtest.bin
"programs/riscvtest.bin" cargado a memoria (84 bytes, binario crudo, inicio en 0x00000000).
> step 2
0x00000000:  00500113   li sp, 5
            sp   (x2): 0x00000000 -> 0x00000005
0x00000004:  00c00193   li gp, 12
            gp   (x3): 0x00000000 -> 0x0000000C
> run
[Programa terminado en 0x00000050 despues de 19 instrucciones, codigo 0]
> mem 100 103
Memoria (0x00000064-0x00000067): 19 00 00 00
```

## Features

**Obligatorios**

- Las 37 instrucciones RV32I del anexo A (loads, stores, aritméticas, lógicas, shifts,
  comparaciones, `lui`, `auipc`, branches, `jal`, `jalr`).
- Memoria plana byte-addressable de 32 bits en formato **little-endian**.
- Carga de binarios crudos en la dirección `0x00000000`.
- Ejecución paso a paso e inspección de PC, registros y memoria.

**Adicionales**

- `ecall` con servicios estilo SPIM/CPUlator (número en `a7`): 1 print_int, 4 print_string,
  5 read_int, 8 read_string, 10 exit, 11 print_char, 12 read_char, 17/93 exit con código,
  34 print_hex, 36 print_unsigned.
- Carga directa de archivos **`.elf`** (RISC-V, 32 bits): se copian los segmentos `PT_LOAD`
  y el PC empieza en el *entry point*.
- **Desensamblador** con nombres ABI y pseudoinstrucciones comunes (`li`, `mv`, `j`, `ret`, `nop`).
- **Breakpoints**, `ebreak`, y detección de fin de programa (`j .` / salto a sí mismo).
- Errores explícitos: instrucción inválida, PC desalineado, `ecall` no soportada.

## Estructura

```text
src/memory.hpp    memoria paginada (páginas de 4 KiB creadas al escribir), little-endian
src/decode.hpp    separa los campos de la instrucción y arma los inmediatos I/S/B/U/J
src/cpu.cpp       estado (pc, x0..x31, memoria) y step(): fetch -> decode -> execute
src/disasm.cpp    desensamblador
src/loader.cpp    carga de binario crudo y ELF32
src/main.cpp      intérprete de comandos
programs/         programas de prueba (.s, .bin y .elf) y build.sh para ensamblarlos
```

## Programas de prueba

| Programa | Resultado esperado | Verificado |
|---|---|---|
| `riscvtest.bin` | escribe 25 (0x19) en la dirección 100 | ✔ |
| `quicksort.bin` | deja {1, 2, 3, 4, 6, 8, 9} en 0x1000 | ✔ |
| `symmetric_tree.bin` | termina con `a0 = 1` | ✔ |
| `hola_ecall.bin` | imprime texto, lee n e imprime 1+2+…+n | ✔ |
| `test_todas.bin` | prueba las 37 instrucciones; imprime "TODAS LAS PRUEBAS PASARON" | ✔ |
| `quicksort.elf` | igual que quicksort, pero cargado desde ELF | ✔ |

Los `.bin` se pueden generar con CPUlator (anexo B del enunciado) o con `programs/build.sh`
(usa clang + lld).
