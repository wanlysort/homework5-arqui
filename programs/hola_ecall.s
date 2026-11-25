# hola_ecall.s - Ejemplo de ecall (servicios estilo SPIM/CPUlator, numero en a7)
# Pide un numero n e imprime la suma 1 + 2 + ... + n
.global _start
_start:
    la   a0, msg_hola
    li   a7, 4               # print_string
    ecall
    la   a0, msg_pide
    li   a7, 4
    ecall
    li   a7, 5               # read_int -> a0
    ecall
    mv   t0, a0              # t0 = n
    li   t1, 0               # t1 = suma
loop:
    blez t0, fin
    add  t1, t1, t0
    addi t0, t0, -1
    j    loop
fin:
    la   a0, msg_res
    li   a7, 4
    ecall
    mv   a0, t1
    li   a7, 1               # print_int
    ecall
    li   a0, '\n'
    li   a7, 11              # print_char
    ecall
    li   a7, 10              # exit
    ecall

msg_hola: .asciz "Hola desde RISC-V!\n"
msg_pide: .asciz "Ingresa un numero n: "
msg_res:  .asciz "La suma 1+2+...+n es: "
