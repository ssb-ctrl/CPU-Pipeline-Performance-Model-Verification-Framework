# first 10 Fibonacci numbers -> memory[0..36]
addi x1, x0, 0      # a
addi x2, x0, 1      # b
addi x3, x0, 0      # address
addi x4, x0, 40     # end address
loop:
sw   x1, 0(x3)
add  x5, x1, x2
add  x1, x0, x2
add  x2, x0, x5
addi x3, x3, 4
bne  x3, x4, loop
halt
