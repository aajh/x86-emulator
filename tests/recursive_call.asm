bits 16

mov ax, 4
call fun
hlt

fun:
sub ax, 1
jz return
call fun
return:
ret

