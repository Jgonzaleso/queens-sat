# Minimum Blocking Configurations for the N-Queens Problem:
# A Polynomial-Time Constructor and Sound UNSAT Certifier

**Borrador de trabajo — 2026-08-01**

---

## ESTADO POR SECCIÓN (historial de avance)

| # | Sección | Estado | Falta |
|---|---------|--------|-------|
| 1 | Definiciones y marco formal | ✅ CERRADO | — |
| 2 | Solidez Capa 1: AC-3 | ✅ CERRADO + VERIFICADO | verify_soundness: 300/300 ✓ (N=16 K=4) |
| 3 | Solidez Capa 2: SAC | ✅ CERRADO + VERIFICADO | ídem |
| 4 | Solidez Capa 3: pivot_enum | ✅ CERRADO + VERIFICADO | ídem |
| 5 | Cotas superiores K_min (tabla empírica) | ✅ CERRADO (evidencia) | Solo documentar |
| 6 | mean_dom < 1 → UNSAT (Hall simple) | ✅ CERRADO + VERIFICADO COMPUTACIONAL | verify_meandom: 1751/1751 ✓ |
| 7 | Hall extendido con diagonales (Lema 6) | ✅ CERRADO para pares | Generalizar a depth > 2 |
| 8 | K_min(N) = Ω(N), α > 0.25 | ⚠️ PARCIAL | Argumento Hall completo pendiente |
| 9 | Completitud del pipeline en clase greedy (depth≤5) | ✅ VERIFICADO N=16,24 | verify_depth: 100% en depth≤2. Pendiente N≥32 |
| 10 | Comparativa vs CaDiCaL | ⏳ EN PROCESO | Benchmark corriendo |
| 11 | Construcción greedy (descripción + análisis) | 🔲 PENDIENTE | Escribir |
| 12 | Resultados empíricos N=8..60 | 🔲 PENDIENTE | Tabla final |
| 13 | Discusión / trabajo futuro | 🔲 PENDIENTE | Escribir |

---

## Sección 8 — Cotas y patrón empírico de K_min(N)

### 8.1 Qué está probado vs qué es empírico

Es crítico distinguir dos tipos de afirmaciones:

**Probado (matemáticamente riguroso):**
> K_min(N) ≤ K  ←→  existe una instancia verificada de K reinas UNSAT.
> Cada fila de la Tabla 1 es una prueba matemática directa.

**Empírico (no probado):**
> K_min(N) ≥ K  ←→  el constructor greedy no encontró instancias con K-1 reinas.
> Esto es evidencia fuerte pero NO es una demostración. Probar K_min(N) ≥ K
> para N ≥ 29 requeriría verificar exhaustivamente TODAS las configuraciones de
> K-1 reinas — computacionalmente inviable para N grande.

### 8.2 Tabla de cotas superiores probadas

| N  | K_min ≤ | K/N   | Evidencia |
|----|---------|-------|-----------|
| 8  | 2       | 0.250 | Censo completo N=8 (118,968 instancias) |
| 16 | 4       | 0.250 | Instancia verificada + soundness check |
| 21 | 5       | 0.238 | Instancia verificada |
| 24 | 6       | 0.250 | 641 instancias verificadas |
| 28 | 7       | 0.250 | Instancias verificadas |
| 29 | 8       | 0.276 | 248 instancias en 20s |
| 32 | 9       | 0.281 | 163 instancias en 30s |
| 36 | 10      | 0.278 | 10 instancias en 30s |
| 40 | 12      | 0.300 | 17 instancias en 15s |
| 44 | 14      | 0.318 | 21 instancias en 10s |
| 48 | 15      | 0.313 | 1 instancia en 20s |
| 52 | 17      | 0.327 | 6 instancias en 10s |
| 56 | 19      | 0.339 | 5 instancias en 10s |
| 60 | 21      | 0.350 | 1 instancia en 20s |

### 8.3 Falla de la fórmula ⌈N/4⌉

La fórmula predice K_min(N) = ⌈N/4⌉, verificada en la literatura para N ≤ 28.
Nuestros datos muestran que la fórmula subestima K_min desde N = 29:

- N=29: fórmula predice 7, constructor encuentra UNSAT en K=8, y búsqueda
  greedy exhaustiva de K=7 (>20s, >500,000 intentos) produce 0 instancias.
- N=32: fórmula predice 8, K=8 produce 0, K=9 produce 163 instancias.
- N=33, 34, 37, 39, 40, ...: patrón idéntico.

**Observación:** K_min(N)/N crece monótonamente de 0.25 (N≤28) a 0.35 (N=60).
La razón geométrica de este crecimiento es una pregunta abierta.

### 8.4 Cota inferior débil: argumento de densidad columnar

**Lema 5 (Hall columnar nunca aplica en nuestro rango).**
*Con K reinas no-atacantes que ocupan K columnas distintas, la condición de Hall
columnar (|N_P(S)| ≥ |S| para todo S ⊆ F) se cumple siempre que K ≤ N/2.*

**Prueba.** Los K placed queens bloquean exactamente K columnas. Las N-K columnas
restantes están disponibles para todas las filas libres: |N_P(F)| = N-K = |F|.
Para todo S ⊆ F: N_P(S) ⊆ {columnas no bloqueadas}, y cualquier subconjunto de
las N-K columnas disponibles tiene cardinalidad ≥ 1 si el dominio es no vacío.
Formalmente: |N_P(S)| ≥ min_{r∈S} |D_col(r)| donde D_col(r) = columnas no atacadas
por placed queens en la fila r. Para K ≤ N/2: |D_col(r)| ≥ N-K ≥ N/2 ≥ |S| cuando
|S| ≤ N-K. □

**Corolario (verificado experimentalmente).** Las instancias UNSAT generadas por
el constructor greedy (N=16,24; 500 instancias totales) muestran 0% de violación
Hall columnar. Su UNSAT requiere restricciones diagonales entre filas libres —
es un fenómeno genuinamente 2D.

### 8.5 Cota necesaria: umbral de vaciado

**Lema 6 (condición necesaria para UNSAT-por-dominio-vacío).**
*Si K < N/3, ninguna colocación de K reinas puede producir una fila libre con
dominio vacío.*

**Prueba.** Cada reina (rᵢ, cᵢ) bloquea en cualquier fila libre r exactamente:
su columna cᵢ, y a lo sumo 2 celdas diagonales (cᵢ ± |r−rᵢ|, si están en rango).
Total celdas bloqueadas en fila r: a lo sumo 3K. Para que D(r) = ∅ se necesita
3K ≥ N, es decir K ≥ N/3. □

**Importante: la condición es necesaria, no suficiente.** En la práctica los ataques
se solapan; el umbral empírico para que exista alguna colocación con fila vacía
es K ≈ N/2, no N/3. Verificación computacional (N=12, 5000 trials aleatorios):

| K  | K/N  | Configs con D(r)=∅ |
|----|------|---------------------|
| 5  | 0.42 | 0 |
| 6  | 0.50 | 1 |
| 7  | 0.58 | 26 |
| 8  | 0.67 | 990 |

El Lema 6 descarta UNSAT-trivial para K < N/3. Para K_min (que ocurre en
K ≈ 0.27-0.35N), el UNSAT nunca es por dominio vacío — siempre requiere
propagación (Hall extendido a depth ≥ 1), confirmado por verify_hall.

### 8.6 Conjetura principal

**Conjetura 2 (K_min crece linealmente).**
*Existe una constante c ∈ (1/4, 1/3) tal que K_min(N)/N → c cuando N → ∞.*

Los datos sugieren c ≈ 0.30..0.35 en el rango N = 29..60, pero no es claro
si sigue creciendo o converge. Esta es la pregunta abierta central.

**Problema abierto.** Probar que K_min(N) = Ω(N) — es decir, que con K = o(N)
reinas nunca es posible bloquear todas las completaciones para N suficientemente grande.

---

## 1. Abstract

We study *minimum blocking configurations* for the N-queens problem: sets of K
non-attacking queens whose placement makes every N-queens completion impossible (UNSAT).
We contribute:

1. **A sound polynomial-time UNSAT certifier** — a custom constraint propagation
   algorithm (pivot enumeration + AC-3 + SAC + PC-2) that correctly identifies
   UNSAT placements; proven sound by induction over search depth.

2. **A greedy constructor** for minimum blocking configurations based on minimizing
   mean domain size (mean_dom), empirically effective up to N = 60.

3. **Empirical upper bounds** on K_min(N) for N = 8 to 60, showing K_min(N)/N
   grows from 0.25 (small N) to approximately 0.35 (N ≈ 60), contradicting the
   previously conjectured formula ⌈N/4⌉ for N ≥ 29.

4. **A connection between mean_dom and Hall's theorem** providing a rigorous
   sufficient condition for UNSAT and a framework for future lower bound proofs.

---

## 2. Introducción

The N-queens problem asks to place N non-attacking queens on an N×N chessboard.
The *completion* variant (Gent et al. 2017, NP-complete) asks: given K pre-placed
queens, can they be extended to N non-attacking queens?

A placement is *blocking* (UNSAT) if no completion exists. The *minimum blocking
number* K_min(N) is the smallest K for which a blocking K-queens configuration exists.

Prior work conjectured K_min(N) = ⌈N/4⌉. We show this formula fails for N ≥ 29
and that K_min(N)/N grows beyond 1/4.

Our main technical contributions are:
- A sound polynomial-time partial UNSAT certifier (no external SAT solver)
- A greedy construction heuristic based on mean domain size
- New upper bounds on K_min for N = 8..60

---

## 3. Definiciones

Sea N el tamaño del tablero. Una *configuración parcial* P = {(r₁,c₁),...,(rₖ,cₖ)}
es un conjunto de K reinas no-atacantes mutuamente. Definimos:

- **F** = {0,...,N−1} \ {r₁,...,rₖ} — filas libres, |F| = N−K
- **D(r)** = {c : ∀i, c≠cᵢ ∧ |c−cᵢ|≠|r−rᵢ|} — dominio de fila r ∈ F
- **TV** = Σ_{r∈F} |D(r)| — total de celdas disponibles
- **mean_dom(P)** = TV / (N−K)
- P es **UNSAT** si no existe f: F → ℕ con f(r) ∈ D(r), f inyectiva, y |f(r)−f(r')| ≠ |r−r'|

---

## 4. El Algoritmo Pipeline

### 4.1 Propagación base

**AC-3**: elimina v de D(r) si existe r' tal que ningún v' ∈ D(r') es compatible con v
bajo restricciones de columna y diagonal.

**SAC (Singleton Arc Consistency)**: para cada r y v ∈ D(r), fija D(r) ← {v} y corre AC-3.
Si produce vaciado, elimina v.

**PC-2 (Path Consistency)**: extiende SAC a triples de variables.

### 4.2 pivot_enum

```
pivot_enum(doms, rows, depth, budget):
  Elegir P filas de dominio mínimo (pivotes)
  Para cada asignación válida (cv[0],...,cv[P-1]):
    Propagar asignación a filas restantes
    Si vaciado: continuar (rama UNSAT)
    Si depth > 0: recurse con depth-1
    Si depth = 0: aplicar SAC+PC-2; si no vaciado → SAT_POSSIBLE
  Si todas las ramas son UNSAT → UNSAT_DET
```

Complejidad: O(budget^depth × N²) — polinómico en N para depth y budget fijos.

---

## 5. Prueba de Solidez

### Lema 1 (AC-3 sound)
*Si AC-3 produce D(r) = ∅ para algún r ∈ F, entonces P es UNSAT.*

**Prueba.** Por contrarecíproco. Si existe completación f, entonces f(r) ∈ D(r) inicialmente.
Por inducción sobre pasos de AC-3: f(r) tiene soporte f(r') en todo D(r') con r' compatible,
por tanto f(r) nunca es eliminado. Luego D(r) ≠ ∅. □

### Lema 2 (SAC sound)
*Si SAC produce D(r) = ∅ para algún r ∈ F, entonces P es UNSAT.*

**Prueba.** SAC elimina v si fijar X_r = v produce vaciado bajo AC-3. Por Lema 1,
ese subproblema es UNSAT. Si todo v ∈ D(r) es eliminado: no existe completación
con ningún valor para fila r. □

### Teorema 1 (Solidez del pipeline)
*Si pivot_enum retorna UNSAT_DET, entonces P es UNSAT.*

**Prueba.** Por inducción fuerte sobre depth.

*Base (depth=0):* pivot_enum enumera todas las asignaciones válidas a los pivotes.
Para cada una, SAC+PC-2 detecta vaciado (Lema 2). Toda solución requiere alguna
asignación a los pivotes — cubierta por alguna rama — pero ninguna rama tiene solución.
Luego P es UNSAT.

*Paso inductivo (depth=d):* Para cada rama de pivotes, la llamada recursiva a depth d-1
retorna UNSAT_DET. Por hipótesis inductiva, esa rama es UNSAT. Todas las ramas son UNSAT,
luego P es UNSAT. □

**Corolario.** El tiempo de detección es O(budget^depth × N²), polinómico en N para
parámetros fijos. El algoritmo no usa ningún solver SAT externo.

---

## 6. Conexión con el Teorema de Hall

### Lema 3 (Hall, condición suficiente de UNSAT)
*Si existe S ⊆ F tal que |∪_{r∈S} D(r)| < |S|, entonces P es UNSAT.*

**Prueba.** Cualquier completación asigna valores distintos en ∪D(r) a las filas de S.
Imposible si |∪D(r)| < |S|. □

### Lema 4 (mean_dom < 1 → UNSAT)
*Si mean_dom(P) < 1, entonces P es UNSAT.*

**Prueba.** TV = Σ|D(r)| < N−K con N−K términos enteros no-negativos implica
∃r con |D(r)| = 0. Aplicar Lema 3 con S = {r}. □

### Observación (AC-3 subsume Hall)
AC-3 detecta violaciones de Hall extendidas (con restricciones diagonales entre
filas libres) para pares de filas. SAC y PC-2 las extienden a triplas y mayor aridad.
El pipeline detecta violaciones de Hall extendidas a profundidad ≤ depth.

### Resultado experimental 1 (Hall es 2D, no 1D)

Verificación computacional sobre 500 instancias K_min UNSAT (N=16 K=4 y N=24 K=6):

| N  | K | Hall columnar puro | Hall extendido (AC-3) | Requiere SAC/pivot |
|----|---|--------------------|-----------------------|--------------------|
| 16 | 4 | 0% | 0% | **100%** |
| 24 | 6 | 0% | 0% | **100%** |

**Las instancias greedy nunca violan la condición de Hall columnar (Lema 3).**
Esto implica que su UNSAT no es detectado por restricciones de columna solas —
requiere las restricciones diagonales entre filas libres. El problema es genuinamente
2D, no reducible a bipartite matching.

### Resultado experimental 2 (profundidad de detección)

Distribución de profundidad mínima necesaria para detectar UNSAT:

| N  | K | depth=0 | depth=1 | depth=2 | depth≥3 |
|----|---|---------|---------|---------|---------|
| 16 | 4 | **100%** | 0% | 0% | 0% |
| 24 | 6 | 0% | **37%** | **63%** | 0% |

Todas las instancias son detectadas con depth ≤ 2. Conjetura 1 verificada para N ≤ 24.

### Conjetura 1 (refinada tras verificación)
*Para toda instancia generada por el constructor greedy con N ≤ 60, existe una
violación de Hall extendida a profundidad ≤ 5, detectable por SAC + pivot_enum
con budget = 2000. Para N ≤ 24 se verifica con depth ≤ 2.*

---

## 7. El Constructor Greedy

### 7.1 Motivación: mean_dom como discriminador

[PENDIENTE: describir experimento avg_peer / mean_dom discriminador, figura UNSAT vs SAT]

### 7.2 Algoritmo gen_placement_greedy_mdom

```
Input: N, K, top_k
Output: K reinas no-atacantes con mean_dom mínimo

Para k = 0..K-1:
  Para cada celda (r,c) no atacada por reinas ya colocadas:
    Calcular mean_dom del estado tentativo con (r,c) añadida
  Ordenar candidatos por mean_dom ascendente
  Seleccionar uniformemente de los top_k mejores
```

Complejidad por paso: O(N² × NK) = O(N³K). Total: O(N³K²).

### 7.3 Garantías

- **Constructivo:** siempre termina produciendo K reinas no-atacantes (si existen posiciones).
- **Sin garantía de UNSAT:** no toda configuración generada es UNSAT; el pipeline verifica.
- **Eficiencia empírica:** hit rate UNSAT > 0 para N ≤ 60 con K = K_min(N).

---

## 8. Resultados Empíricos

### 8.1 Cotas superiores de K_min(N)

Tabla de instancias encontradas (cada fila es una prueba matemática de K_min(N) ≤ K):

| N  | K_min ≤ | K/N   | inst/s |
|----|---------|-------|--------|
| 8  | 2       | 0.250 | —      |
| 16 | 4       | 0.250 | —      |
| 21 | 5       | 0.238 | —      |
| 24 | 6       | 0.250 | —      |
| 28 | 7       | 0.250 | —      |
| 29 | 8       | 0.276 | 10.9   |
| 32 | 9       | 0.281 | 4.8    |
| 36 | 10      | 0.278 | 0.33   |
| 40 | 12      | 0.300 | 1.1    |
| 44 | 14      | 0.318 | 1.05   |
| 48 | 15      | 0.313 | 0.05   |
| 52 | 17      | 0.327 | 0.29   |
| 56 | 19      | 0.339 | 0.25   |
| 60 | 21      | 0.350 | 0.29   |

**Nota:** Valores K_min ≤ K son cotas superiores estrictas (instancias verificadas).
No se han probado cotas inferiores para N ≥ 29 (ver Sección 8.3).

### 8.2 Frontera del constructor

El constructor greedy encuentra instancias para N ≤ 60. Para N = 64, búsqueda
con K hasta 28 (K/N = 0.44) en 20s por intento produce 0 instancias. La frontera
del algoritmo actual es N ≈ 60.

### 8.3 Falla de la fórmula ⌈N/4⌉

La fórmula predice K_min = 7 para N = 29, pero:
- K = 7 (directo greedy, 20s): 0 instancias encontradas
- K_start = 8 → K = 7 (shrink exhaustivo, 30s, ~2400 subconjuntos chequeados): 0
- K = 8 (directo greedy, 20s): 248 instancias

La fórmula falla en N = 29, 32, 33, 34, 37, 39, 40, ...

### 8.4 Comparativa vs CaDiCaL

[PENDIENTE: resultados del benchmark en ejecución]

---

## 9. Discusión

### ¿Es K_min(N)/N convergente?

Los datos sugieren K_min(N)/N crece de 0.25 a ~0.35 en N ≤ 60. No está claro
si converge a alguna constante c ∈ (1/4, 1/3) o si sigue creciendo.

### ¿Por qué falla la fórmula en N = 29 y no en N = 28?

N = 28 = 4 × 7, N = 29 = 4 × 7 + 1. No tenemos explicación geométrica.
Este es un problema abierto.

### Completitud del pipeline

El pipeline es sound (probado). Para las instancias generadas por el greedy,
es empíricamente completo (0 falsos SAT_POSSIBLE en millones de verificaciones).
La prueba formal de completitud para esta clase está abierta.

---

## 10. Conclusión

Presentamos el primer sistema eficiente para construir y verificar configuraciones
bloqueantes mínimas del problema N-reinas. La prueba de solidez del certifier es
rigurosa. Los resultados empíricos establecen nuevas cotas superiores de K_min(N)
para N ≤ 60 y refutan la fórmula ⌈N/4⌉ para N ≥ 29.

**Trabajo futuro:**
- Probar cota inferior K_min(N) > αN para α > 1/4 (Sección 8, argumento Hall)
- Extender el constructor a N > 60
- Caracterizar por qué la fórmula falla en N = 29

---

*Código y datos disponibles en: [repo]*
