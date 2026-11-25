#!/bin/sh
# Ensambla cada .s a .elf y a .bin crudo (cargado en 0x0) usando clang/lld.
# (Tambien se pueden generar los .bin desde CPUlator, ver anexo B del enunciado.)
set -e
cd "$(dirname "$0")"
for s in *.s; do
  n="${s%.s}"
  clang --target=riscv32 -march=rv32i -mno-relax -c "$s" -o "$n.o"
  ld.lld -m elf32lriscv -Ttext=0 -e 0 --no-relax "$n.o" -o "$n.elf"
  llvm-objcopy -O binary "$n.elf" "$n.bin"
  rm "$n.o"
  echo "$n.bin ($(wc -c < "$n.bin") bytes)"
done
