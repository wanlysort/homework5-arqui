# Arbol simetrico - coloca el arbol completo aplanado {6, 4, 4, 1, 2, 2, 1}
# en 0x1000 y evalua is_symmetric. Al terminar, a0 = 1 (true).
# Hijos del nodo i: 2i+1 (izquierdo) y 2i+2 (derecho).
.global _start
_start:
    li   sp, 0x10000
    li   a0, 0x1000
    li   t0, 6
    sw   t0, 0(a0)
    li   t0, 4
    sw   t0, 4(a0)
    li   t0, 4
    sw   t0, 8(a0)
    li   t0, 1
    sw   t0, 12(a0)
    li   t0, 2
    sw   t0, 16(a0)
    li   t0, 2
    sw   t0, 20(a0)
    li   t0, 1
    sw   t0, 24(a0)
    li   a1, 7               # n = 7 nodos
    call is_symmetric
_end:
    j    _end

# is_symmetric(a0 = arr, a1 = n) -> a0 = 1 si es simetrico
is_symmetric:
    addi sp, sp, -16
    sw   ra, 12(sp)
    li   a2, 1               # hijo izquierdo de la raiz
    li   a3, 2               # hijo derecho de la raiz
    call solve
    lw   ra, 12(sp)
    addi sp, sp, 16
    ret

# solve(a0 = arr, a1 = n, a2 = l, a3 = r) -> a0
solve:
    bge  a2, a1, ret_true    # l fuera del arbol -> true
    bge  a3, a1, ret_false   # r fuera del arbol -> false
    slli t0, a2, 2
    add  t0, a0, t0
    lw   t0, 0(t0)           # arr[l]
    slli t1, a3, 2
    add  t1, a0, t1
    lw   t1, 0(t1)           # arr[r]
    bne  t0, t1, ret_false   # valores distintos -> false

    addi sp, sp, -16
    sw   ra, 12(sp)
    sw   s0, 8(sp)
    sw   s1, 4(sp)
    sw   s2, 0(sp)
    mv   s0, a2              # s0 = l
    mv   s1, a3              # s1 = r
    mv   s2, a0              # s2 = arr

    slli a2, s0, 1
    addi a2, a2, 1           # 2l + 1
    slli a3, s1, 1
    addi a3, a3, 2           # 2r + 2
    call solve               # par externo
    beqz a0, solve_done

    mv   a0, s2
    slli a2, s0, 1
    addi a2, a2, 2           # 2l + 2
    slli a3, s1, 1
    addi a3, a3, 1           # 2r + 1
    call solve               # par interno
solve_done:
    lw   ra, 12(sp)
    lw   s0, 8(sp)
    lw   s1, 4(sp)
    lw   s2, 0(sp)
    addi sp, sp, 16
    ret
ret_true:
    li   a0, 1
    ret
ret_false:
    li   a0, 0
    ret
