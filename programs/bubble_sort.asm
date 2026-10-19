# bubble sort 8 numbers stored at memory[0..28]  ->  1 2 3 4 5 7 8 9
addi x1, x0, 5
sw x1, 0(x0)
addi x1, x0, 2
sw x1, 4(x0)
addi x1, x0, 9
sw x1, 8(x0)
addi x1, x0, 1
sw x1, 12(x0)
addi x1, x0, 7
sw x1, 16(x0)
addi x1, x0, 3
sw x1, 20(x0)
addi x1, x0, 8
sw x1, 24(x0)
addi x1, x0, 4
sw x1, 28(x0)
addi x2, x0, 28       # x2 = end offset of the current pass
outer:
beq  x2, x0, done
addi x3, x0, 0        # x3 = i (byte offset)
inner:
lw   x4, 0(x3)
lw   x5, 4(x3)
slt  x6, x5, x4       # x6 = 1 if a[i+1] < a[i]
beq  x6, x0, noswap
sw   x5, 0(x3)
sw   x4, 4(x3)
noswap:
addi x3, x3, 4
bne  x3, x2, inner
addi x2, x2, -4
beq  x0, x0, outer    # unconditional jump
done:
halt
