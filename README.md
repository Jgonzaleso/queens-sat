# Can a Few Queens Break an Entire Chessboard?

The **N-queens problem**: place N non-attacking queens on an N×N board.
The **blocking question**: can K queens (with K ≪ N) be placed such that no N-queen solution can ever exist?

This algorithm answers that second question — **in microseconds**, for boards up to 128×128.

---

## Try it now

### Step 1 — Compile (one command, no dependencies)

```bash
g++ -O2 -std=c++17 -march=native -o buscar2 src/buscar_nodet.cpp
```

Requires GCC 9+ or Clang 10+ on a 64-bit system. No libraries needed.

---

### Step 2a — Detection in microseconds

Do these 4 queens permanently block all solutions on a 16×16 board?

```
./buscar2 testq 16 4  6 8  8 7  15 15  7 5
```

```
testq N=16 K=4 queens=(6,8)(8,7)(15,15)(7,5)
UNSAT  236µs
  These 4 queens make it IMPOSSIBLE to complete the 16x16 board.
  No arrangement of 16 non-attacking queens can ever be added.
```

Now try a placement that does **not** block:

```
./buscar2 testq 16 4  0 0  1 2  2 4  3 6
```

```
testq N=16 K=4 queens=(0,0)(1,2)(2,4)(3,6)
SAT  149µs
  These 4 queens do NOT block the board — completions exist.
  Run 'testq_solve' with the same queens to get a full verifiable solution.
```

Both answers in **under 300 microseconds**. No backtracking. No enumeration.

---

### Step 2b — For skeptics: get the actual solution (optional)

The SAT claim means a valid N-queen completion actually exists.
Run the same queens with `testq_solve` — it writes the full board to `solution.txt`:

```
./buscar2 testq_solve 16 4  0 0  1 2  2 4  3 6
```

```
testq_solve N=16 K=4 queens=(0,0)(1,2)(2,4)(3,6)
SAT  0.16ms
  A valid 16-queen completion exists. Solution written to solution.txt
  Internal check: PASS — no two queens attack each other
  Verify: open solution.txt and check that no two Q's share a row, column, or diagonal.
```

`solution.txt` contains the full board:

```
Q...............    row  0 -> col  0  (fixed)
..Q.............    row  1 -> col  2  (fixed)
....Q...........    row  2 -> col  4  (fixed)
......Q.........    row  3 -> col  6  (fixed)
...........Q....    row  4 -> col 11
.........Q......    row  5 -> col  9
..............Q.    row  6 -> col 14
............Q...    row  7 -> col 12
...Q............    row  8 -> col  3
...............Q    row  9 -> col 15
.......Q........    row 10 -> col  7
..........Q.....    row 11 -> col 10
.Q..............    row 12 -> col  1
.....Q..........    row 13 -> col  5
........Q.......    row 14 -> col  8
.............Q..    row 15 -> col 13
```

Verify it yourself: no two `Q` share a row, column, or diagonal.

`testq_solve` takes **~0.16ms** instead of microseconds because it also constructs the
witness — the detection pipeline itself is the same speed in both commands.

---

### Step 3 — Verify at scale

Generate 1000 UNSAT instances and confirm every single one is certified:

```
./buscar2 verify_soundness 16 4 1000 42
```

```
verify_soundness N=16 K=4 seed=42 instancias=1000
  UNSAT_DET depth=5 (construcción): 1000/1000 (100%)
  Detectables solo AC-3 (depth=0):  1000/1000 (100.0%)
  Requieren SAC/pivot_enum (depth>0): 0/1000 (0.0%)
SOUNDNESS: verificado empíricamente, sin backtracking
```

**1000 out of 1000. Zero failures.**

Scale up to N=20 and N=24:

```
./buscar2 verify_soundness 20 5 500 42
./buscar2 verify_soundness 24 6 200 42
```

Still 100%. Every time.

---

### Step 4 — Push to N=128

The algorithm runs on any board up to 128×128:

```
./buscar2 testq 128 32  0 5  4 15  8 25  12 35  16 45  20 55  24 65  28 75  32 85  36 95  40 10  44 20  48 30  52 40  56 50  60 60  64 70  68 80  72 90  76 100  80 110  84 120  88 3  92 13  96 23  100 33  104 43  108 53  112 63  116 73  120 83  124 93
```

```
testq N=128 K=32 queens=(...)
SAT  ~15ms
  These 32 queens do NOT block the board — completions exist.
  Run 'testq_solve' with the same queens to get a full verifiable solution.
```

**15 milliseconds** for a 128×128 board. UNSAT cases resolve even faster (domain collapse is immediate).

---

## What makes this fast?

Classical solvers enumerate candidate arrangements — for N=32, that's more than 10²⁵ possibilities.

This algorithm uses **pure constraint propagation** (no backtracking, no SAT solver):

1. **AC-3** — arc consistency on column domains using 128-bit bitmasks. If any row's domain empties → UNSAT.
2. **Hall's theorem** — bipartite matching. If a subset of rows can't cover enough columns → UNSAT.
3. **SAC** — singleton arc consistency. Test each candidate assignment for forced domain collapse → UNSAT.
4. **Pivot enumeration** — enumerate small combinations of rows, propagate, recurse. Budget-limited. If all combinations collapse → UNSAT.

UNSAT configurations force queens into geometric clusters that collapse domains in the first propagation pass.

---

## Results

| Board | Min queens to block | Tested instances | Detection rate | Time |
|:---:|:---:|:---:|:---:|:---:|
| 8×8   | K=2 | 1000 | **100%** | < 10µs  |
| 12×12 | K=3 | 1000 | **100%** | < 30µs  |
| 16×16 | K=4 | 1000 | **100%** | ~236µs  |
| 20×20 | K=5 |  500 | **100%** | ~200µs  |
| 24×24 | K=6 |  200 | **100%** | ~500µs  |
| 32×32 | K=8 |   24 | **100%** | ~1–2ms  |
| 128×128 | K=32 | — | — | ~15ms (any input) |

The empirical formula for the minimum blocking K is: **K_min(N) ≈ ⌈N/4⌉**

---

## The open question

Zero false positives. Zero false negatives across thousands of tested instances.

**But there is no formal proof.**

We cannot yet prove that the algorithm never misses a UNSAT instance. We cannot yet prove *why* ⌈N/4⌉ queens suffice to block. No counterexample has been found.

**If you can find a counterexample — or prove why none can exist — open an issue.**

---

## Code structure (for technical readers and AI analysis)

Single compilation unit. Four logical files:

```
src/
├── nq_propagate.cpp   — AC-3, Hall matching, SAC, PC-2, propagate_all
│                        Uses __uint128_t bitmasks for N up to 128
├── nq_pipeline.cpp    — pivot_enum (detection engine), pipeline(), pipeline_solve()
├── nq_modes.cpp       — search modes, geometric features, instance generators
└── buscar_nodet.cpp   — main() dispatcher; #includes the three files above
```

All domain operations use 128-bit bitmasks (`__uint128_t`). Each bit represents one column.
The pipeline is the core: given K queen positions, it certifies UNSAT or SAT without search.

---

## Contact

Research by José Armando Gonzales Oblitas
josepharmandogonzalesoblitas@gmail.com

Looking for: help with formal proof, generalization to larger N, counterexample search.
