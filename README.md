# CPU Pipeline Performance Model & Verification Framework

A cycle-by-cycle simulator of a 5-stage pipelined RISC CPU, written in C++. It runs small assembly programs, measures CPI under different hazard-handling options, and checks every run against a simple reference model. A Python script generates random programs to test it.

## Background

A pipelined CPU overlaps instructions. While one instruction executes, the next is being decoded and the one after that is being fetched. The five stages are:

| Stage | Work done |
|---|---|
| IF | fetch the instruction |
| ID | decode it, read registers |
| EX | ALU operation, branch decision, address calculation |
| MEM | load or store |
| WB | write the result to the register file |

Ideally one instruction finishes every cycle, so CPI (cycles per instruction) is 1. In practice, hazards get in the way:

- **Data hazard**: an instruction needs a register value that an earlier instruction has not written yet. Without forwarding, the pipeline stalls until the value is written back. With forwarding, the result is passed directly from the EX/MEM or MEM/WB stage to the input of EX.
- **Load-use hazard**: a `lw` produces its value only after the MEM stage, so an instruction that uses it immediately must still stall for one cycle, even with forwarding.
- **Control hazard**: a branch is resolved in EX, by which time two more instructions have been fetched. If the guess was wrong, those two are flushed (2 cycles lost). The simulator supports three predictors: always not-taken (`nt`), always taken (`taken`), and a table of 2-bit saturating counters (`2bit`).

## Instruction set

`add`, `sub`, `and`, `or`, `slt`, `addi`, `lw`, `sw`, `beq`, `bne`, `nop`, `halt`. There are 32 registers (`x0` is always 0) and 1024 words of memory. Example:

```
addi x1, x0, 10
sw   x1, 0(x0)
lw   x2, 0(x0)
beq  x1, x2, label
```

## Files

| File | Purpose |
|---|---|
| `pipeline_sim.cpp` | assembler, reference model and pipeline |
| `programs/` | test programs: `hazard_demo`, `fibonacci`, `sum_array`, `bubble_sort` |
| `gen_random.py` | generates random programs (branches only jump forward, so they always terminate) |
| `fuzz.py` | runs many random programs and compares against the reference model |
| `Makefile` | builds `pipeline_sim` |

## Build and run

```
make
./pipeline_sim programs/bubble_sort.asm --sweep
./pipeline_sim programs/hazard_demo.asm --trace
./pipeline_sim programs/sum_array.asm --no-forward --predictor 2bit
python3 fuzz.py 300
```

`--sweep` runs all six configurations (forwarding on/off, three predictors). `--trace` prints which instruction is in each stage on every cycle.

## Verification

Each run is compared with a reference model that executes one instruction at a time, with no pipeline. The final registers, memory and instruction count must match. `fuzz.py` does this for random programs in all six configurations.

To check that the test setup can actually catch bugs, `--inject-bug` disables the load-use stall:

| Run | Programs passed |
|---|---|
| normal | 300 / 300 |
| `--inject-bug` | 207 / 300 |

## Results

Bubble sort of 8 numbers (240 instructions):

| Forwarding | Predictor | Cycles | CPI | Mispredicts |
|---|---|---|---|---|
| off | not-taken | 533 | 2.22 | 44 |
| off | taken | 506 | 2.11 | 27 |
| off | 2-bit | 489 | 2.04 | 19 |
| on | not-taken | 361 | 1.50 | 44 |
| on | taken | 327 | 1.36 | 27 |
| on | 2-bit | 311 | 1.30 | 19 |

Forwarding gives the larger gain, cutting stall cycles from about 200 to 28. The predictor then reduces the branch penalty. The 2-bit predictor does best here because the sort is made of loops with a mostly consistent direction. Always-taken wins on the short `fibonacci` loop, since its single loop branch is almost always taken:

| Program | Forwarding | Predictor | CPI |
|---|---|---|---|
| fibonacci | off | not-taken | 1.84 |
| fibonacci | on | not-taken | 1.36 |
| fibonacci | on | taken | 1.11 |

## Limitations

- Single-issue and in-order, with no caches. Memory takes one cycle.
- The branch target is assumed to be known at fetch (as if there were a perfect branch target buffer).
- Small ISA with no multiply, shifts or jumps (an unconditional jump is written as `beq x0, x0, label`).
- Programs are limited to 1024 words of memory.
