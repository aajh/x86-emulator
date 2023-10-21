bits 16

mov bx, 1000

mov byte [bx], 0
mov byte [bx + 1], 0x42

mov ax, [bx]
mov cl, ah
