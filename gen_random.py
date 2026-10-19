#!/usr/bin/env python3
"""Random program generator for fuzz testing. Branches only jump FORWARD, so programs always terminate.
Usage: python3 gen_random.py SEED [LENGTH] > prog.asm"""
import random, sys

def gen(seed, length=40):
    r = random.Random(seed)
    regs = [f"x{i}" for i in range(1, 8)]          # small pool -> many hazards
    lines = [f"addi {x}, x0, {r.randint(-20, 20)}" for x in regs]
    body, labels = [], {}
    for idx in range(length):
        k = r.random()
        if k < 0.40:
            body.append(f"{r.choice(['add','sub','and','or','slt'])} {r.choice(regs)}, {r.choice(regs)}, {r.choice(regs)}")
        elif k < 0.55:
            body.append(f"addi {r.choice(regs)}, {r.choice(regs)}, {r.randint(-9, 9)}")
        elif k < 0.70:
            body.append(f"lw {r.choice(regs)}, {4 * r.randrange(32)}(x0)")
        elif k < 0.85:
            body.append(f"sw {r.choice(regs)}, {4 * r.randrange(32)}(x0)")
        else:
            tgt = min(idx + 1 + r.randint(1, 3), length)
            labels.setdefault(tgt, f"L{tgt}")
            body.append(f"{r.choice(['beq','bne'])} {r.choice(regs)}, {r.choice(regs)}, {labels[tgt]}")
    for idx, ins in enumerate(body):
        if idx in labels: lines.append(labels[idx] + ":")
        lines.append(ins)
    if length in labels: lines.append(labels[length] + ":")
    lines.append("halt")
    return "\n".join(lines) + "\n"

if __name__ == "__main__":
    sys.stdout.write(gen(int(sys.argv[1]), int(sys.argv[2]) if len(sys.argv) > 2 else 40))
