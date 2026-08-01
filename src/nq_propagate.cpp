// nq_propagate.cpp — primitivos, AC-3, Hall, Régin GAC-AllDiff, propagate_all, triple
// Incluido por buscar_nodet.cpp (single translation unit via #include).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>
#include <utility>

using u64     = uint64_t;
using bitmask = __uint128_t;
static const int MAXN = 128;

// ─── GLOBALES ────────────────────────────────────────────────────────────────

int g_qr[MAXN], g_qc[MAXN]; // estado greedy / no-goods

bool g_hall_enabled    = true;
bool g_triple_enabled  = true;
int  g_max_combos      = 2000;
bool g_debug_propagate = false;
int  g_pipeline_depth  = 2;
bool g_mcv_enabled     = false; // MCV: ordena columnas por impacto en dominio restante

static inline int ctz64(u64 d)  { return __builtin_ctzll(d); }
static inline int pop64(u64 d)  { return __builtin_popcountll(d); }

// Helpers para bitmask de 128 bits
static inline int pop_bm(bitmask d) {
    return __builtin_popcountll((u64)d) + __builtin_popcountll((u64)(d >> 64));
}
static inline int ctz_bm(bitmask d) {
    u64 lo = (u64)d;
    return lo ? __builtin_ctzll(lo) : 64 + __builtin_ctzll((u64)(d >> 64));
}
static inline bitmask col_bit(int c)  { return (bitmask)1 << c; }
static inline bitmask col_mask(int N) {
    if (N >= 128) return (bitmask)-1;
    return ((bitmask)1 << N) - 1;
}

// ─── PRIMITIVOS ──────────────────────────────────────────────────────────────

bitmask available_bits(int row, int N, const int* qr, const int* qc, int k) {
    bitmask mask = col_mask(N);
    for (int i = 0; i < k; i++) {
        int diff = row - qr[i];
        mask &= ~col_bit(qc[i]);
        int p = qc[i] + diff; if (p >= 0 && p < N) mask &= ~col_bit(p);
        p     = qc[i] - diff; if (p >= 0 && p < N) mask &= ~col_bit(p);
    }
    return mask;
}

bitmask attack_mask(int rq, int cq, int s, int N) {
    bitmask m = col_bit(cq);
    int diff = s - rq;
    int p = cq + diff; if (p >= 0 && p < N) m |= col_bit(p);
    p     = cq - diff; if (p >= 0 && p < N) m |= col_bit(p);
    return m;
}

// ─── AC-3 ────────────────────────────────────────────────────────────────────

bool ac3_bits(bitmask* doms, const int* rows, int n, int N) {
    static const int Q_CAP = MAXN * MAXN * 16;
    static std::pair<int16_t,int16_t> q_buf[Q_CAP];
    int qh = 0, qt = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j) q_buf[qt++] = {(int16_t)i, (int16_t)j};
    while (qh < qt) {
        auto [ii, jj] = q_buf[qh++];
        int ri = rows[ii], rj = rows[jj];
        int diff = abs(ri - rj);
        bitmask d_rj = doms[jj], d_ri = doms[ii], to_rem = 0, tmp = d_ri;
        while (tmp) {
            int ci = ctz_bm(tmp); tmp &= tmp - 1;
            bitmask incompat = col_bit(ci);
            int p = ci + diff; if (p < N)  incompat |= col_bit(p);
            p     = ci - diff; if (p >= 0) incompat |= col_bit(p);
            if ((d_rj & ~incompat) == 0) to_rem |= col_bit(ci);
        }
        if (to_rem) {
            doms[ii] = d_ri & ~to_rem;
            if (!doms[ii]) return true;
            for (int kk = 0; kk < n; kk++)
                if (kk != ii && kk != jj) {
                    if (qt >= Q_CAP) return false; // buffer lleno: conservador, no UNSAT
                    q_buf[qt++] = {(int16_t)kk, (int16_t)ii};
                }
        }
    }
    return false;
}

// ─── SAC ─────────────────────────────────────────────────────────────────────

bool sac_bits(bitmask* doms, const int* rows, int n, int N) {
    bitmask test[MAXN];
    bool changed = true;
    while (changed) {
        changed = false;
        for (int ii = 0; ii < n; ii++) {
            int r = rows[ii];
            bitmask d = doms[ii], to_rem = 0, tmp = d;
            while (tmp) {
                int c = ctz_bm(tmp); tmp &= tmp - 1;
                memcpy(test, doms, n * sizeof(bitmask));
                test[ii] = col_bit(c);
                bool bad = false;
                for (int jj = 0; jj < n && !bad; jj++) {
                    if (jj == ii) continue;
                    bitmask new_dom = doms[jj] & ~attack_mask(r, c, rows[jj], N);
                    if (!new_dom) bad = true; else test[jj] = new_dom;
                }
                if (bad) { to_rem |= col_bit(c); continue; }
                if (ac3_bits(test, rows, n, N)) to_rem |= col_bit(c);
            }
            if (to_rem) {
                doms[ii] = d & ~to_rem;
                changed = true;
                if (!doms[ii]) return true;
            }
        }
    }
    return false;
}

// ─── PC-2 ────────────────────────────────────────────────────────────────────
// Para N>64 solo corre AC-3 (la tabla supp sería 33 MB para N=128).

static u64 supp64[64][64][64];

bool pc2_bits(bitmask* doms, const int* rows, int n, int N) {
    if (ac3_bits(doms, rows, n, N)) return true;
    if (N > 64) return false;
    // Convertir a u64 para el PC-2 interno (seguro: N≤64 → todos los bits caben en u64)
    u64 d64[64];
    for (int i = 0; i < n; i++) d64[i] = (u64)doms[i];
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) { memset(supp64[i][j], 0, 64 * sizeof(u64)); continue; }
            int diff = abs(rows[i] - rows[j]);
            memset(supp64[i][j], 0, 64 * sizeof(u64));
            u64 tmp = d64[i];
            while (tmp) {
                int c1 = ctz64(tmp); tmp &= tmp - 1;
                u64 incompat = (u64)1 << c1;
                int p = c1 + diff; if (p < N)  incompat |= (u64)1 << p;
                p     = c1 - diff; if (p >= 0) incompat |= (u64)1 << p;
                supp64[i][j][c1] = d64[j] & ~incompat;
            }
        }
    }
    bool changed = true; int iters = 0;
    while (changed && iters < 20) {
        changed = false; iters++;
        for (int i = 0; i < n; i++) {
            for (int j = i+1; j < n; j++) {
                for (int kk = 0; kk < n; kk++) {
                    if (kk == i || kk == j) continue;
                    u64 to_rem = 0, tmp = d64[i];
                    while (tmp) {
                        int c1 = ctz64(tmp); tmp &= tmp - 1;
                        u64 c2m = supp64[i][j][c1], c3c1 = supp64[i][kk][c1];
                        if (!c3c1) { to_rem |= (u64)1 << c1; continue; }
                        bool has_ext = false;
                        u64 cm = c2m;
                        while (cm && !has_ext) {
                            int c2 = ctz64(cm); cm &= cm - 1;
                            if (supp64[j][kk][c2] & c3c1) has_ext = true;
                        }
                        if (!has_ext) to_rem |= (u64)1 << c1;
                    }
                    if (to_rem) {
                        changed = true;
                        d64[i] &= ~to_rem;
                        if (!d64[i]) {
                            for (int x = 0; x < n; x++) doms[x] = d64[x];
                            return true;
                        }
                        for (int m = 0; m < n; m++) {
                            if (m == i) continue;
                            u64 t2 = to_rem;
                            while (t2) { int c1 = ctz64(t2); t2 &= t2-1; supp64[i][m][c1] = 0; }
                            u64 t3 = d64[m];
                            while (t3) { int cm = ctz64(t3); t3 &= t3-1; supp64[m][i][cm] &= d64[i]; }
                        }
                    }
                }
            }
        }
    }
    for (int i = 0; i < n; i++) doms[i] = d64[i];
    return false;
}

// ─── BIPARTITE HALL ──────────────────────────────────────────────────────────

static int bip_mc[MAXN];

static bool bip_aug(int i, const bitmask* doms, int N, bool* vis) {
    bitmask d = doms[i];
    while (d) {
        int c = ctz_bm(d); d &= d - 1;
        if (vis[c]) continue;
        vis[c] = true;
        if (bip_mc[c] < 0 || bip_aug(bip_mc[c], doms, N, vis)) {
            bip_mc[c] = i; return true;
        }
    }
    return false;
}

static bool bipartite_hall(const bitmask* doms, int ns, int N) {
    memset(bip_mc, -1, N * sizeof(int));
    bool vis[MAXN];
    int matched = 0;
    for (int i = 0; i < ns; i++) {
        memset(vis, 0, N * sizeof(bool));
        if (bip_aug(i, doms, N, vis)) matched++;
    }
    return matched < ns;
}

// ─── HALL DIAGONAL ───────────────────────────────────────────────────────────

static int bip_mc_ud[4*MAXN];
static int bip_mc_dd[4*MAXN];

static bool bip_aug_ud(int i, const bitmask* doms, const int* rows, int N, bool* vis) {
    bitmask d = doms[i];
    while (d) {
        int c = ctz_bm(d); d &= d - 1;
        int ud = rows[i] + c;
        if (vis[ud]) continue;
        vis[ud] = true;
        if (bip_mc_ud[ud] < 0 || bip_aug_ud(bip_mc_ud[ud], doms, rows, N, vis)) {
            bip_mc_ud[ud] = i; return true;
        }
    }
    return false;
}

static bool bip_aug_dd(int i, const bitmask* doms, const int* rows, int N, bool* vis) {
    bitmask d = doms[i];
    while (d) {
        int c = ctz_bm(d); d &= d - 1;
        int dd = (c - rows[i]) + (N - 1);
        if (vis[dd]) continue;
        vis[dd] = true;
        if (bip_mc_dd[dd] < 0 || bip_aug_dd(bip_mc_dd[dd], doms, rows, N, vis)) {
            bip_mc_dd[dd] = i; return true;
        }
    }
    return false;
}

static bool hall_updiag(const bitmask* doms, const int* rows, int ns, int N) {
    int range = 2 * N - 1;
    memset(bip_mc_ud, -1, range * sizeof(int));
    bool vis[4*MAXN];
    int matched = 0;
    for (int i = 0; i < ns; i++) {
        memset(vis, 0, range * sizeof(bool));
        if (bip_aug_ud(i, doms, rows, N, vis)) matched++;
    }
    return matched < ns;
}

static bool hall_downdiag(const bitmask* doms, const int* rows, int ns, int N) {
    int range = 2 * N - 1;
    memset(bip_mc_dd, -1, range * sizeof(int));
    bool vis[4*MAXN];
    int matched = 0;
    for (int i = 0; i < ns; i++) {
        memset(vis, 0, range * sizeof(bool));
        if (bip_aug_dd(i, doms, rows, N, vis)) matched++;
    }
    return matched < ns;
}

// ─── RÉGIN GAC-AllDiff ────────────────────────────────────────────────────────
// regan_updiag / regan_downdiag deshabilitados (bug de solidez en Kuhn matching).

struct TarjanState {
    int disc[3*MAXN], low[3*MAXN], scc[3*MAXN];
    int stk[3*MAXN], stk_top;
    bool on_stk[3*MAXN];
    int timer, n_scc;
    int head[3*MAXN], nxt[9*MAXN*MAXN], to_[9*MAXN*MAXN], n_edges;
    void init(int n_nodes) {
        memset(disc, -1, n_nodes * sizeof(int));
        memset(on_stk, 0, n_nodes * sizeof(bool));
        memset(head, -1, n_nodes * sizeof(int));
        stk_top = timer = n_scc = n_edges = 0;
    }
    void add_edge(int u, int v) { to_[n_edges]=v; nxt[n_edges]=head[u]; head[u]=n_edges++; }
    void dfs_rec(int u) {
        struct Frame { int u, it; };
        static Frame call_stk[3*MAXN]; int cs_top = 0;
        disc[u] = low[u] = timer++;
        stk[stk_top++] = u; on_stk[u] = true;
        call_stk[cs_top++] = {u, head[u]};
        while (cs_top > 0) {
            auto& [cu, it] = call_stk[cs_top-1];
            if (it == -1) {
                cs_top--;
                if (cs_top > 0) low[call_stk[cs_top-1].u] = std::min(low[call_stk[cs_top-1].u], low[cu]);
                if (low[cu] == disc[cu]) {
                    while (true) { int v = stk[--stk_top]; on_stk[v] = false; scc[v] = n_scc; if (v == cu) break; }
                    n_scc++;
                }
            } else {
                int v = to_[it]; it = nxt[it];
                if (disc[v] < 0) {
                    disc[v] = low[v] = timer++;
                    stk[stk_top++] = v; on_stk[v] = true;
                    call_stk[cs_top++] = {v, head[v]};
                } else if (on_stk[v]) {
                    low[cu] = std::min(low[cu], disc[v]);
                }
            }
        }
    }
    void run(int n_nodes) { for (int i = 0; i < n_nodes; i++) if (disc[i] < 0) dfs_rec(i); }
} static ts;

static bool regan_col(bitmask* doms, const int* rows, int ns, int N) {
    memset(bip_mc, -1, N * sizeof(int));
    bool vis[MAXN];
    int matched = 0, match_row[MAXN]; memset(match_row, -1, ns * sizeof(int));
    for (int i = 0; i < ns; i++) {
        memset(vis, 0, N * sizeof(bool));
        if (bip_aug(i, doms, N, vis)) matched++;
    }
    if (matched < ns) return true;
    for (int c = 0; c < N; c++) if (bip_mc[c] >= 0) match_row[bip_mc[c]] = c;
    int src = ns + N, total = ns + N + 1;
    ts.init(total);
    for (int c = 0; c < N; c++) {
        int ri = bip_mc[c];
        if (ri >= 0) ts.add_edge(ns + c, ri);
        else          ts.add_edge(src, ns + c);
    }
    for (int i = 0; i < ns; i++) {
        int mc = match_row[i];
        bitmask tmp = doms[i]; while (tmp) {
            int c = ctz_bm(tmp); tmp &= tmp - 1;
            if (c != mc) ts.add_edge(i, ns + c);
        }
    }
    ts.run(total);
    for (int i = 0; i < ns; i++) {
        int mc = match_row[i];
        bitmask to_rem = 0, tmp = doms[i]; while (tmp) {
            int c = ctz_bm(tmp); tmp &= tmp - 1;
            if (c != mc && ts.scc[i] != ts.scc[ns + c]) to_rem |= col_bit(c);
        }
        if (to_rem) { doms[i] &= ~to_rem; if (!doms[i]) return true; }
    }
    return false;
}

// ─── PROPAGACIÓN COMPLETA ────────────────────────────────────────────────────

static bool propagate_all(bitmask* doms, const int* rows, int ns, int N) {
    auto chk = [&](const char* name, bool result, int iter) -> bool {
        if (result && g_debug_propagate) fprintf(stderr, "UNSAT at %s iter=%d\n", name, iter);
        return result;
    };
    for (int iter = 0; iter < 8; iter++) {
        bitmask saved[MAXN]; memcpy(saved, doms, ns * sizeof(bitmask));
        if (chk("ac3_1",    ac3_bits(doms, rows, ns, N),     iter)) return true;
        if (chk("hall_col", bipartite_hall(doms, ns, N),     iter)) return true;
        if (chk("hall_up",  hall_updiag(doms, rows, ns, N),  iter)) return true;
        if (chk("hall_dn",  hall_downdiag(doms, rows, ns, N),iter)) return true;
        if (chk("regan_col",regan_col(doms, rows, ns, N),    iter)) return true;
        if (chk("ac3_2",    ac3_bits(doms, rows, ns, N),     iter)) return true;
        // regan_updiag / regan_downdiag disabled: soundness bug
        bool stable = true;
        for (int i = 0; i < ns; i++) if (doms[i] != saved[i]) { stable = false; break; }
        if (stable) return false;
    }
    return false;
}

// ─── TRIPLE CONSISTENCY ──────────────────────────────────────────────────────

static bool triple_unsat(const bitmask* doms, const int* rows, int ns, int N) {
    for (int i = 0; i < ns - 2; i++)
        for (int j = i + 1; j < ns - 1; j++) {
            int dij = abs(rows[i] - rows[j]);
            for (int k = j + 1; k < ns; k++) {
                int dik = abs(rows[i] - rows[k]);
                int djk = abs(rows[j] - rows[k]);
                bool found = false;
                bitmask tmp_i = doms[i];
                while (tmp_i && !found) {
                    int ci = ctz_bm(tmp_i); tmp_i &= tmp_i - 1;
                    bitmask cj_avail = doms[j] & ~col_bit(ci);
                    int p = ci + dij; if (p < N)  cj_avail &= ~col_bit(p);
                    p     = ci - dij; if (p >= 0) cj_avail &= ~col_bit(p);
                    if (!cj_avail) continue;
                    bitmask ck_ci = doms[k] & ~col_bit(ci);
                    p = ci + dik; if (p < N)  ck_ci &= ~col_bit(p);
                    p = ci - dik; if (p >= 0) ck_ci &= ~col_bit(p);
                    bitmask tmp_j = cj_avail;
                    while (tmp_j && !found) {
                        int cj = ctz_bm(tmp_j); tmp_j &= tmp_j - 1;
                        bitmask ck = ck_ci & ~col_bit(cj);
                        p = cj + djk; if (p < N)  ck &= ~col_bit(p);
                        p = cj - djk; if (p >= 0) ck &= ~col_bit(p);
                        if (ck) found = true;
                    }
                }
                if (!found) return true;
            }
        }
    return false;
}
