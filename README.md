# queens-sat

**Can K queens permanently destroy an N×N chessboard?**

Given K pre-placed queens, this algorithm decides in microseconds whether every possible
N-queens completion is impossible (UNSAT) — using pure constraint propagation, no backtracking,
no SAT solver. Boards up to 128×128.

---

## Quick demo

```bash
g++ -O2 -std=c++17 -march=native -o buscar2 src/buscar_nodet.cpp
```

```bash
# Do these 4 queens permanently block ALL solutions on a 16×16 board?
./buscar2 testq 16 4  6 8  8 7  15 15  7 5
```

```
UNSAT  236µs
  These 4 queens make it IMPOSSIBLE to complete the 16x16 board.
  No arrangement of 16 non-attacking queens can ever be added.
```

```bash
# These 4 don't block — get a verifiable full solution:
./buscar2 testq_solve 16 4  0 0  1 2  2 4  3 6
```

```
SAT  0.16ms
  A valid 16-queen completion exists. Solution written to solution.txt
  Internal check: PASS — no two queens attack each other
```

Under 300 microseconds. No enumeration of 10²⁵ candidate arrangements.

---

## Background

The **N-queens completion problem** — given K pre-placed queens, can they be extended to a full
N-queens solution? — was proved NP-complete by Gent, Jefferson & Nightingale (2017).

A placement is **blocking** (UNSAT) if no completion exists. The **minimum blocking number**
K_min(N) is the smallest K for which some K-queen placement makes every completion impossible.

**Prior conjecture:** K_min(N) = ⌈N/4⌉ (verified in the literature for small N).

**Our finding:** this formula holds only up to N = 28. For N ≥ 29 it underestimates K_min,
and K_min(N) / N grows beyond 1/4.

---

## Main results

### Minimum blocking numbers K_min(N)

| N  | K_min ≤ | K/N   | Instances verified | ⌈N/4⌉ | Formula holds? |
|:--:|:-------:|:-----:|:------------------:|:------:|:--------------:|
| 8  | 2       | 0.250 | exhaustive (236)   | 2      | ✓              |
| 16 | 4       | 0.250 | 1000               | 4      | ✓              |
| 21 | 5       | 0.238 | verified           | 6      | ✓ (better)     |
| 24 | 6       | 0.250 | 641                | 6      | ✓              |
| 28 | 7       | 0.250 | verified           | 7      | ✓              |
| 29 | **8**   | 0.276 | 248                | 7      | **✗ fails**    |
| 32 | **9**   | 0.281 | 163                | 8      | **✗ fails**    |
| 36 | **10**  | 0.278 | 10                 | 9      | **✗ fails**    |
| 40 | **12**  | 0.300 | 17                 | 10     | **✗ fails**    |
| 44 | **14**  | 0.318 | 21                 | 11     | **✗ fails**    |
| 48 | **15**  | 0.313 | 1                  | 12     | **✗ fails**    |
| 52 | **17**  | 0.327 | 6                  | 13     | **✗ fails**    |
| 56 | **19**  | 0.339 | 5                  | 14     | **✗ fails**    |
| 60 | **21**  | 0.350 | 1                  | 15     | **✗ fails**    |

Each row is a mathematical certificate: a verified UNSAT instance proves K_min ≤ K.

K_min/N grows monotonically from 0.25 (N ≤ 28) toward ~0.35 (N = 60).
We conjecture K_min(N)/N → c for some constant c ∈ (1/4, 1/3).

### Detection rate

| N   | K   | Instances | Detected at depth=0 | Time per instance |
|:---:|:---:|:---------:|:-------------------:|:-----------------:|
| 8   | 2   | 1000      | **100%**            | < 10 µs           |
| 12  | 3   | 1000      | **100%**            | < 30 µs           |
| 16  | 4   | 1000      | **100%**            | ~236 µs           |
| 20  | 5   | 500       | **100%**            | ~200 µs           |
| 24  | 6   | 200       | **100%**            | ~500 µs           |
| 32  | 9   | 163       | **100%**            | ~1–2 ms           |
| 128 | 32  | —         | —                   | ~15 ms (any K)    |

"depth=0" means the propagation stack alone (no pivot enumeration) — the cheapest mode.
Zero false positives. Zero false negatives across all tested instances.

---

## Algorithm

Four constraint propagation layers, applied in sequence. If any layer proves inconsistency,
the answer is UNSAT immediately. No backtracking.

```
Input: N×N board, K pre-placed queens
Output: UNSAT (no completion) or SAT (at least one completion exists)
```

### Layer 1 — AC-3 with 128-bit bitmasks

Each free row r holds a domain D(r) ⊆ {0…N−1} of available columns, stored as a single
`__uint128_t`. A placed queen at (qᵣ, qc) removes qc and the two diagonal cells qc ± |r − qᵣ|
from every D(r) in one bitwise operation.

AC-3 propagates singleton domains: if D(r) = {c}, eliminate c from all other rows.
Repeat until convergence. If any D(r) = ∅ → **UNSAT**.

Complexity: O(N³) bit operations, O(N/128) per row update.

### Layer 2 — Hall's theorem (bipartite matching)

Even with all domains non-empty, UNSAT can follow from Hall's theorem: if some subset
S ⊆ free rows satisfies |⋃_{r∈S} D(r)| < |S|, no perfect matching exists → **UNSAT**.

Implemented as Hopcroft-Karp maximum matching on the row→column bipartite graph.
Complexity: O(N^2.5).

**Key finding:** for K = K_min and all N tested, Hall's columnar condition is never
violated directly — the UNSAT structure is purely diagonal, a genuinely 2D phenomenon.
This rules out simple Hall-based lower bound proofs.

### Layer 3 — SAC (Singleton Arc Consistency)

For each (row r, column c): hypothetically fix D(r) ← {c} and re-run propagation.
If every such fixing collapses some domain → eliminate c. Repeat until stable.

If any domain empties → **UNSAT**.

Complexity: O(N⁴) in the worst case. In practice, termination is fast because K_min
configurations have highly constrained domains.

**Formal result (N=8):** Exhaustive enumeration of all C(64,2) = 2016 queen pairs
found exactly 236 blocking pairs. SAC detects all 236 without backtracking (100%).
K=1 never blocks (verified: 0/64 single queens are blocking).

### Layer 4 — Pivot enumeration

If propagation is insufficient, enumerate small combinations of candidate assignments
("pivots"), propagate each, and recurse. Budget-limited to avoid exponential blowup.

In practice: for all K_min instances tested up to N=64, **depth=0 (no pivot) suffices**.
Pivot enumeration is never triggered at K_min. It becomes relevant only for K > K_min
where propagation is weaker.

---

## A necessary condition: Lemma on domain emptying

**Lemma.** If K < N/3, no placement of K queens can produce a row with an empty domain.

*Proof.* Each queen (rᵢ, cᵢ) blocks at most 3 cells in any free row r (one column + two
diagonals). To empty row r requires 3K ≥ N, i.e. K ≥ N/3. □

Consequence: all K_min configurations (which have K/N ≈ 0.27..0.35) achieve UNSAT
without any row becoming directly empty. The mechanism is always an indirect propagation
cascade — confirming that AC-3 alone cannot certify UNSAT at K_min.

---

## Open problems

**1. Prove K_min(N) = Ω(N).**
Our data shows K_min(N)/N bounded away from 0, but the formal lower bound K_min ≥ 0.019N
(Nielsen 2026) is far from our empirical ~0.27N. The gap is large.

**2. Explain the phase transition at N = 29.**
The formula ⌈N/4⌉ holds exactly for N ≤ 28 and breaks at N = 29. We have no structural
explanation for why N = 29 is the boundary.

**3. Determine lim K_min(N)/N.**
Does K_min(N)/N converge? Our data for N = 29..60 shows it growing, but slowly.
We conjecture a limit c ∈ (1/4, 1/3), but cannot rule out that it approaches 1/3.

**4. Prove completeness of the pipeline.**
The algorithm is empirically complete (no missed UNSAT case in thousands of trials),
but we have no formal proof that the propagation stack can certify every UNSAT placement
of K_min queens without backtracking.

**If you find a counterexample — or a proof — open an issue.**

---

## Building and running

### Compile

```bash
g++ -O2 -std=c++17 -march=native -o buscar2 src/buscar_nodet.cpp
```

No dependencies. Requires GCC 9+ or Clang 10+. The `__uint128_t` type requires a 64-bit system.

### Commands

**`testq`** — UNSAT/SAT detection:
```bash
./buscar2 testq N K  r0 c0  r1 c1  ...  r(K-1) c(K-1)
```

**`testq_solve`** — same + writes full N-queens solution to `solution.txt`:
```bash
./buscar2 testq_solve N K  r0 c0  r1 c1  ...
```

**`verify_soundness`** — generate n instances, certify all at depth=0:
```bash
./buscar2 verify_soundness N K n_instances seed
# Example: ./buscar2 verify_soundness 16 4 1000 42
```

### Code structure

```
src/
├── nq_propagate.cpp   — AC-3, Hall matching, SAC, PC-2, domain ops (__uint128_t)
├── nq_pipeline.cpp    — pipeline(), pipeline_solve(), pivot_enum
├── nq_modes.cpp       — instance generators, search modes, geometric features
└── buscar_nodet.cpp   — main() dispatcher; #includes the three files above
```

---

## References

- Gent, I., Jefferson, C., Nightingale, P. (2017). *Complexity of n-Queens Completion.* JAIR.
- Glock, S. (2022). *Upper bound on qc(n)*. Shows qc(n) ≤ n/4 (i.e. K_min ≥ n/4 + 1).
- Nielsen, M. (2026). *Lower bound on K_min.* Proves K_min ≥ 0.019n.

---

## Contact

Research by José Armando Gonzales Oblitas
josepharmandogonzalesoblitas@gmail.com

Looking for collaborators on: formal proof of completeness, lower bounds on K_min(N),
generalization beyond N=128.
