# test_todas.s - Prueba automatica de las 37 instrucciones RV32I del anexo A.
# Cada prueba compara un resultado con el valor esperado.
# Al final: a0 = 0 si TODO salio bien; si no, a0 = numero de la prueba que fallo.
# Tambien imprime el resultado con ecall.

.macro CHECK reg, expected
    addi s11, s11, 1          # numero de prueba
    li   t6, \expected
    bne  \reg, t6, fail
.endm

.global _start
_start:
    li   s11, 0
    li   sp, 0x10000

    # --- Aritmetica y logica con inmediato ---
    li   t0, 10
    addi t1, t0, -15          ; CHECK t1, -5
    slti t1, t0, 11           ; CHECK t1, 1
    slti t1, t0, -1           ; CHECK t1, 0
    li   t2, -1
    sltiu t1, t2, 5           ; CHECK t1, 0     # 0xFFFFFFFF no es < 5 sin signo
    sltiu t1, t0, -1          ; CHECK t1, 1     # 10 < 0xFFFFFFFF sin signo
    xori t1, t0, 0xF          ; CHECK t1, 5
    ori  t1, t0, 0x5          ; CHECK t1, 15
    andi t1, t0, 0x6          ; CHECK t1, 2
    slli t1, t0, 4            ; CHECK t1, 160
    li   t2, 0x80000000
    srli t1, t2, 4            ; CHECK t1, 0x08000000
    srai t1, t2, 4            ; CHECK t1, 0xF8000000

    # --- Registro-registro ---
    li   t0, 7
    li   t2, -3
    add  t1, t0, t2           ; CHECK t1, 4
    sub  t1, t2, t0           ; CHECK t1, -10
    li   t3, 3
    sll  t1, t0, t3           ; CHECK t1, 56
    slt  t1, t2, t0           ; CHECK t1, 1     # -3 < 7
    sltu t1, t2, t0           ; CHECK t1, 0     # 0xFFFFFFFD no es < 7
    xor  t1, t0, t3           ; CHECK t1, 4
    li   t4, 0x80000010
    srl  t1, t4, t3           ; CHECK t1, 0x10000002
    sra  t1, t4, t3           ; CHECK t1, 0xF0000002
    or   t1, t0, t3           ; CHECK t1, 7
    and  t1, t0, t3           ; CHECK t1, 3
    li   t3, 33               # solo cuentan los 5 bits bajos -> shift de 1
    sll  t1, t0, t3           ; CHECK t1, 14

    # --- lui / auipc ---
    lui  t1, 0x12345          ; CHECK t1, 0x12345000
here:
    auipc t1, 1
    la   t2, here
    sub  t1, t1, t2           ; CHECK t1, 0x1000

    # --- Stores y loads (little-endian, signo) ---
    li   a0, 0x2000
    li   t0, 0x80FF7F01
    sw   t0, 0(a0)
    lw   t1, 0(a0)            ; CHECK t1, 0x80FF7F01
    lbu  t1, 0(a0)            ; CHECK t1, 0x01  # byte mas bajo primero
    lb   t1, 2(a0)            ; CHECK t1, -1    # 0xFF con signo
    lbu  t1, 2(a0)            ; CHECK t1, 0xFF
    lh   t1, 2(a0)            ; CHECK t1, 0xFFFF80FF
    lhu  t1, 2(a0)            ; CHECK t1, 0x80FF
    li   t0, 0xAB
    sb   t0, 1(a0)
    lw   t1, 0(a0)            ; CHECK t1, 0x80FFAB01
    li   t0, 0x1234
    sh   t0, 2(a0)
    lw   t1, 0(a0)            ; CHECK t1, 0x1234AB01

    # --- Branches ---
    li   t0, -1
    li   t2, 1
    li   t1, 0
    beq  t0, t0, 1f
    j    fail
1:  bne  t0, t2, 1f
    j    fail
1:  blt  t0, t2, 1f           # -1 < 1 con signo
    j    fail
1:  bge  t2, t0, 1f
    j    fail
1:  bltu t2, t0, 1f           # 1 < 0xFFFFFFFF sin signo
    j    fail
1:  bgeu t0, t2, 1f
    j    fail
1:  beq  t0, t2, fail         # no se deben tomar
    bne  t0, t0, fail
    blt  t2, t0, fail
    bge  t0, t2, fail
    bltu t0, t2, fail
    bgeu t2, t0, fail
    addi s11, s11, 1

    # --- jal / jalr ---
    jal  ra, func
    CHECK a1, 99
    la   t0, func
    jalr ra, 0(t0)
    CHECK a1, 99
    li   x0, 123              # x0 siempre es 0
    CHECK x0, 0

    # --- Exito ---
    la   a0, ok_msg
    li   a7, 4
    ecall
    li   a0, 0
    li   a7, 93               # exit(a0)
    ecall

func:
    li   a1, 99
    ret

fail:
    la   a0, fail_msg
    li   a7, 4
    ecall
    mv   a0, s11
    li   a7, 1
    ecall
    li   a0, '\n'
    li   a7, 11
    ecall
    mv   a0, s11
    li   a7, 93
    ecall

ok_msg:   .asciz "TODAS LAS PRUEBAS PASARON\n"
fail_msg: .asciz "FALLO la prueba numero "
