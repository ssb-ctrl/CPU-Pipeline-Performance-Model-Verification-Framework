# store 1..8 into memory, then sum it (answer: x2 = 36)
addi x1, x0, 1
addi x3, x0, 0
addi x4, x0, 32
init:
sw   x1, 0(x3)
addi x1, x1, 1
addi x3, x3, 4
bne  x3, x4, init
addi x3, x0, 0
addi x2, x0, 0
sum:
lw   x5, 0(x3)
add  x2, x2, x5     # load-use hazard every iteration
addi x3, x3, 4
bne  x3, x4, sum
halt
