# Tiny program to watch hazards with --trace
addi x1, x0, 10
addi x2, x1, 5      # RAW hazard on x1 (back-to-back)
sw   x2, 0(x0)
lw   x3, 0(x0)
add  x4, x3, x2     # load-use hazard on x3
halt
