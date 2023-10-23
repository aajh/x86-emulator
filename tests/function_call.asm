bits 16

mov word [1000], 0
call fun
call fun
mov ax, [1000]
hlt

fun:
add word [1000], 1
ret

