"""
Análisis exhaustivo N=8 K=2:
Para cada par de reinas bloqueante, diagnostica QUÉ mecanismo prueba UNSAT:
  - DIRECT: alguna fila queda vacía directamente (AC-3 paso 1)
  - HALL:   Hall violation en bipartite matching (ninguna fila vacía pero no hay matching)
  - CASCADE: requiere propagación iterativa AC-3

También verifica que K=1 nunca bloquea y caracteriza el patrón geométrico.
"""

from itertools import combinations, product

N = 8

def attacks(r1, c1, r2, c2):
    return c1 == c2 or abs(r1-r2) == abs(c1-c2)

def available_cols(r, queens):
    """Columnas disponibles para fila r dado queens ya colocadas."""
    cols = set(range(N))
    for qr, qc in queens:
        cols.discard(qc)                  # misma columna
        d = abs(r - qr)
        cols.discard(qc + d)              # diagonal +
        cols.discard(qc - d)              # diagonal -
    return cols

def ac3(queens):
    """
    AC-3 hasta convergencia sobre las filas libres.
    Retorna (doms, collapsed) donde doms[r] = set de cols disponibles
    y collapsed=True si alguna fila quedó vacía.
    """
    used_rows = {qr for qr, qc in queens}
    free_rows = [r for r in range(N) if r not in used_rows]

    # Dominios iniciales
    doms = {r: available_cols(r, queens) for r in free_rows}

    changed = True
    while changed:
        changed = False
        for r in free_rows:
            if len(doms[r]) == 0:
                return doms, True
            if len(doms[r]) == 1:
                # Singleton: propaga
                c = next(iter(doms[r]))
                for r2 in free_rows:
                    if r2 == r: continue
                    before = len(doms[r2])
                    doms[r2].discard(c)
                    d = abs(r - r2)
                    doms[r2].discard(c + d)
                    doms[r2].discard(c - d)
                    if len(doms[r2]) < before:
                        changed = True
                    if len(doms[r2]) == 0:
                        return doms, True
    return doms, False

def bipartite_matching(doms, free_rows):
    """
    Matching máximo en grafo bipartito filas->columnas.
    Retorna tamaño del matching máximo.
    """
    match_col = {}  # col -> row
    match_row = {}  # row -> col

    def augment(r, visited):
        for c in doms[r]:
            if c not in visited:
                visited.add(c)
                if c not in match_col or augment(match_col[c], visited):
                    match_col[c] = r
                    match_row[r] = c
                    return True
        return False

    for r in free_rows:
        augment(r, set())
    return len(match_row)

def hall_violation(doms, free_rows):
    """
    Encuentra el conjunto S mínimo que viola Hall: |N(S)| < |S|.
    Retorna (S, N(S)) o None si no hay violación.
    """
    for size in range(1, len(free_rows)+1):
        for S in combinations(free_rows, size):
            NS = set()
            for r in S:
                NS |= doms[r]
            if len(NS) < len(S):
                return list(S), list(NS)
    return None

def is_unsat(queens):
    """Verifica UNSAT via backtracking completo — única forma correcta."""
    used_rows = {qr for qr, qc in queens}
    free_rows = sorted(r for r in range(N) if r not in used_rows)
    placed = list(queens)

    def bt(idx):
        if idx == len(free_rows):
            return True
        r = free_rows[idx]
        for c in range(N):
            ok = all(qc != c and abs(qr-r) != abs(qc-c) for qr, qc in placed)
            if ok:
                placed.append((r, c))
                if bt(idx + 1):
                    return True
                placed.pop()
        return False

    return not bt(0)

# ── Generar todos los pares no-atacantes K=2 en N=8 ──────────────────────────

all_queens_unsat = []
all_positions = [(r, c) for r in range(N) for c in range(N)]

pairs_checked = 0
for (r1,c1), (r2,c2) in combinations(all_positions, 2):
    if r1 == r2: continue          # misma fila: no es una colocación válida en N-queens
    if attacks(r1,c1, r2,c2): continue  # se atacan entre sí
    pairs_checked += 1
    if is_unsat([(r1,c1),(r2,c2)]):
        all_queens_unsat.append(((r1,c1),(r2,c2)))

print(f"N={N}, K=2")
print(f"Pares no-atacantes revisados: {pairs_checked}")
print(f"Pares UNSAT encontrados: {len(all_queens_unsat)}")
print()

# ── Clasificar cada par UNSAT ─────────────────────────────────────────────────

def ac3_from_doms(doms_in, free_rows):
    """AC-3 sobre dominios dados (copia). Retorna (doms, collapsed)."""
    doms = {r: set(v) for r, v in doms_in.items()}
    changed = True
    while changed:
        changed = False
        for r in free_rows:
            if not doms[r]: return doms, True
            if len(doms[r]) == 1:
                c = next(iter(doms[r]))
                for r2 in free_rows:
                    if r2 == r: continue
                    before = len(doms[r2])
                    doms[r2].discard(c)
                    d = abs(r - r2)
                    doms[r2].discard(c + d)
                    doms[r2].discard(c - d)
                    if len(doms[r2]) < before: changed = True
                    if not doms[r2]: return doms, True
    return doms, False

def sac_check(doms_in, free_rows):
    """
    SAC: para cada valor posible en cada fila, intenta fijar y propagar.
    Si fijarlo siempre colapsa → elimínalo.
    Retorna (doms_after_sac, collapsed).
    """
    doms = {r: set(v) for r, v in doms_in.items()}
    changed = True
    while changed:
        changed = False
        for r in free_rows:
            to_remove = []
            for c in list(doms[r]):
                # Fijar r=c y propagar
                tmp = {r2: set(doms[r2]) for r2 in free_rows}
                tmp[r] = {c}
                # Eliminar c de las otras filas (col + diag)
                for r2 in free_rows:
                    if r2 == r: continue
                    d = abs(r - r2)
                    tmp[r2].discard(c)
                    tmp[r2].discard(c + d)
                    tmp[r2].discard(c - d)
                _, col = ac3_from_doms(tmp, free_rows)
                if col:
                    to_remove.append(c)
            for c in to_remove:
                doms[r].discard(c)
                changed = True
            if not doms[r]:
                return doms, True
    return doms, False

counts = {"DIRECT_INITIAL": 0, "DIRECT_AC3": 0, "HALL_AC3": 0,
          "SAC": 0, "HALL_SAC": 0, "NEED_BT": 0}
hall_set_sizes = []
collapse_rows = {}

for (r1,c1),(r2,c2) in all_queens_unsat:
    queens = [(r1,c1),(r2,c2)]
    used_rows = {r1, r2}
    free_rows = [r for r in range(N) if r not in used_rows]

    # 1. Dominios iniciales
    doms0 = {r: available_cols(r, queens) for r in free_rows}
    if any(not doms0[r] for r in free_rows):
        counts["DIRECT_INITIAL"] += 1
        continue

    # 2. AC-3 convergencia
    doms_ac3, collapsed = ac3_from_doms(doms0, free_rows)
    if collapsed:
        counts["DIRECT_AC3"] += 1
        continue

    # 3. Hall en dominios post-AC3
    if bipartite_matching(doms_ac3, free_rows) < len(free_rows):
        counts["HALL_AC3"] += 1
        hv = hall_violation(doms_ac3, free_rows)
        if hv: hall_set_sizes.append(len(hv[0]))
        continue

    # 4. SAC
    doms_sac, collapsed = sac_check(doms_ac3, free_rows)
    if collapsed:
        counts["SAC"] += 1
        continue

    # 5. Hall en dominios post-SAC
    if bipartite_matching(doms_sac, free_rows) < len(free_rows):
        counts["HALL_SAC"] += 1
        continue

    counts["NEED_BT"] += 1

print("=== CLASIFICACIÓN DE MECANISMOS ===")
total = len(all_queens_unsat)
for k, v in counts.items():
    print(f"  {k:20s}: {v:4d} / {total} ({100*v/total:.1f}%)")
print()

if hall_set_sizes:
    from collections import Counter
    print(f"  Hall set sizes: {dict(Counter(hall_set_sizes))}")
    print()

print("=== FILAS QUE COLAPSAN (direct) ===")
for r in sorted(collapse_rows):
    print(f"  fila {r}: {collapse_rows[r]} veces  (dist_centro={abs(r-3.5):.1f})")
print()

# ── Patrón geométrico de los pares UNSAT ─────────────────────────────────────

print("=== PATRÓN GEOMÉTRICO ===")
centers = []
spans_r = []
spans_c = []
dists = []
for (r1,c1),(r2,c2) in all_queens_unsat:
    cr = (r1+r2)/2
    cc = (c1+c2)/2
    centers.append((cr,cc))
    spans_r.append(abs(r1-r2))
    spans_c.append(abs(c1-c2))
    dists.append(((cr-3.5)**2 + (cc-3.5)**2)**0.5)

avg_sr  = sum(spans_r)/len(spans_r)
avg_sc  = sum(spans_c)/len(spans_c)
avg_d   = sum(dists)/len(dists)
max_d   = max(dists)
in_win  = sum(1 for d in dists if d <= N/4+0.5)  # dentro de ventana K=2 del centro

print(f"  row span promedio:    {avg_sr:.2f}  (esperado ≈ {N//4})")
print(f"  col span promedio:    {avg_sc:.2f}")
print(f"  dist al centro prom:  {avg_d:.2f}")
print(f"  dist al centro max:   {max_d:.2f}")
print(f"  en ventana central:   {in_win}/{total} ({100*in_win/total:.1f}%)")
print()

# ── Verificar K=1 nunca bloquea ───────────────────────────────────────────────

k1_unsat = 0
for r, c in all_positions:
    if is_unsat([(r, c)]):
        k1_unsat += 1
print(f"=== K=1 UNSAT instances: {k1_unsat} / 64  (debe ser 0) ===")
print()

# ── Muestra algunos pares con su diagnóstico detallado ───────────────────────

print("=== MUESTRA DE 5 PARES CON DIAGNÓSTICO ===")
for (r1,c1),(r2,c2) in all_queens_unsat[:5]:
    queens = [(r1,c1),(r2,c2)]
    free_rows = [r for r in range(N) if r not in {r1,r2}]
    doms0 = {r: available_cols(r, queens) for r in free_rows}
    doms_ac3, _ = ac3(queens)
    print(f"  queens=({r1},{c1})({r2},{c2})")
    for r in free_rows:
        d0 = sorted(doms0[r])
        d1 = sorted(doms_ac3.get(r, set()))
        flag = " ← VACÍO" if not d1 else (" ← singleton" if len(d1)==1 else "")
        print(f"    row {r}: initial={d0}  →  ac3={d1}{flag}")
    print()
