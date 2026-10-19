# Project 2 – 5-Stage Pipeline CPU Simulator + Verification Fuzzer

## Idea
Real CPUs overlap instructions like an assembly line: **IF → ID → EX → MEM → WB**. This causes *hazards* (an instruction needs a result that is not ready yet, or a branch changes the next instruction). The simulator runs small assembly programs cycle by cycle, measures **CPI**, and — like a real verification team — checks every run against a simple **reference model** that executes one instruction at a time.

## Files
- `pipeline_sim.cpp` – assembler + reference model + pipeline
- `programs/*.asm` – hazard demo, fibonacci, sum array, bubble sort
- `gen_random.py` + `fuzz.py` – random test generator and regression runner (verification flow)

## Instruction set (12 instructions)
`add sub and or slt` (R-type) · `addi` · `lw rd, imm(rs)` · `sw rs, imm(rs)` · `beq bne` (label) · `nop` · `halt`. Registers `x0..x31` (`x0` = 0). Memory is 1024 words, byte-addressed (multiples of 4).

## Run
```
make
./pipeline_sim programs/bubble_sort.asm --sweep           # 6 configs side by side
./pipeline_sim programs/hazard_demo.asm --trace           # cycle-by-cycle pipeline picture
./pipeline_sim programs/sum_array.asm --no-forward --predictor 2bit
python3 fuzz.py 300                                       # 300 random programs x 6 configs
python3 fuzz.py 300 --inject-bug                          # break load-use stall; fuzzer must catch it
```

## Hazard handling (what the code does)
| Problem | Without fix | Fix implemented |
|---|---|---|
| Data (RAW) | stall in ID until the producer has written back | **Forwarding**: ALU result copied from EX/MEM or MEM/WB straight to EX |
| Load-use | even forwarding can't help (data arrives after MEM) | **1-cycle stall**, then forward from MEM/WB |
| Control | branch resolves in EX → 2 wrong-path instructions fetched | **Predictors**: `nt` (always not-taken), `taken`, `2bit` (table of 2-bit counters); mispredict flushes 2 instructions |

## Sample results (bubble_sort)
| Forwarding | Predictor | Cycles | CPI |
|---|---|---|---|
| off | nt | 533 | 2.22 |
| on | nt | 361 | 1.50 |
| on | 2bit | 311 | 1.30 |

Forwarding removes most data stalls; the 2-bit predictor then cuts branch penalties. The ideal CPI is 1.0.

## Verification story (say this in interviews)
- **Golden model**: `runReference` is simple enough to trust.
- **Stimulus**: `gen_random.py` makes random programs with many hazards (small register pool).
- **Checker**: final registers, memory and instruction count must match in all 6 configs.
- **Bug injection**: `--inject-bug` disables the load-use stall → ~30% of random programs fail. Proof the testbench actually catches bugs.
This is the same loop (stimulus → DUT → scoreboard → coverage) used in UVM verification, without needing Verilog.

## Interview questions to prepare
- Why does `lw` followed by a dependent `add` still stall with forwarding?
- Why does the pipeline write the register file before ID reads it in the same cycle?
- Why is a 2-bit counter better than 1-bit for loops?
- What is CPI and what limits it to 1.0 here? (superscalar/out-of-order go beyond)

## Extension ideas (optional)
Add `jal`; add a cache from Project 1 to the MEM stage (miss = stall N cycles) — this connects both projects; add functional-coverage counters (which hazard types were exercised).
