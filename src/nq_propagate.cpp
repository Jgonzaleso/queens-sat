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

using u64 = uint64_t;
static const int MAXN = 64;

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

// ─── PRIMITIVOS ──────────────────────────────────────────────────────────────

u64 available_bits(int row, int N, const int* qr, const int* qc, int k) {
    u64 mask = (N >= 64) ? ~(u64)0 : (((u64)1 << N) - 1);
    for (int i = 0; i < k; i++) {
        mask &= ~((u64)1 << qc[i]);
        int diff = row - qr[i];
        int p = qc[i] + diff; if (p >= 0 && p < N) mask &= ~((u64)1 << p);
        p     = qc[i] - diff; if (p >= 0 && p < N) mask &= ~((u64)1 << p);
    }
    return mask;
}

u64 attack_mask(int rq, int cq, int s, int N) {
    u64 m = (u64)1 << cq;
    int diff = s - rq;
    int p = cq + diff; if (p >= 0 && p < N) m |= (u64)1 << p;
    p     = cq - diff; if (p >= 0 && p < N) m |= (u64)1 << p;
    return m;
}

// ─── AC-3 ────────────────────────────────────────────────────────────────────

bool ac3_bits(u64* doms, const int* rows, int n, int N) {
    static std::pair<int8_t,int8_t> q_buf[MAXN * MAXN * 16];
    int qh = 0, qt = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j) q_buf[qt++] = {(int8_t)i, (int8_t)j};
    while (qh < qt) {
        auto [ii, jj] = q_buf[qh++];
        int ri = rows[ii], rj = rows[jj];
        int diff = abs(ri - rj);
        u64 d_rj = doms[jj], d_ri = doms[ii], to_rem = 0, tmp = d_ri;
        while (tmp) {
            int ci = ctz64(tmp); tmp &= tmp - 1;
            u64 incompat = (u64)1 << ci;
            int p = ci + diff; if (p < N)  incompat |= (u64)1 << p;
            p     = ci - diff; if (p >= 0) incompat |= (u64)1 << p;
            if ((d_rj & ~incompat) == 0) to_rem |= (u64)1 << ci;
        }
        if (to_rem) {
            doms[ii] = d_ri & ~to_rem;
            if (!doms[ii]) return true;
            for (int kk = 0; kk < n; kk++)
                if (kk != ii && kk != jj) q_buf[qt++] = {(int8_t)kk, (int8_t)ii};
        }
    }
    return false;
}

// ─── SAC ─────────────────────────────────────────────────────────────────────

bool sac_bits(u64* doms, const int* rows, int n, int N) {
    u64 test[MAXN];
    bool changed = true;
    while (changed) {
        changed = false;
        for (int ii = 0; ii < n; ii++) {
            int r = rows[ii];
            u64 d = doms[ii], to_rem = 0, tmp = d;
            while (tmp) {
                int c = ctz64(tmp); tmp &= tmp - 1;
                memcpy(test, doms, n * sizeof(u64));
                test[ii] = (u64)1 << c;
                bool bad = false;
                for (int jj = 0; jj < n && !bad; jj++) {
                    if (jj == ii) continue;
                    u64 ns = doms[jj] & ~attack_mask(r, c, rows[jj], N);
                    if (!ns) bad = true; else test[jj] = ns;
                }
                if (bad) { to_rem |= (u64)1 << c; continue; }
                if (ac3_bits(test, rows, n, N)) to_rem |= (u64)1 << c;
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

static u64 supp[MAXN][MAXN][MAXN];

bool pc2_bits(u64* doms, const int* rows, int n, int N) {
    if (ac3_bits(doms, rows, n, N)) return true;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) { memset(supp[i][j], 0, MAXN * sizeof(u64)); continue; }
            int diff = abs(rows[i] - rows[j]);
            memset(supp[i][j], 0, MAXN * sizeof(u64));
            u64 tmp = doms[i];
            while (tmp) {
                int c1 = ctz64(tmp); tmp &= tmp - 1;
                u64 incompat = (u64)1 << c1;
                int p = c1 + diff; if (p < N)  incompat |= (u64)1 << p;
                p     = c1 - diff; if (p >= 0) incompat |= (u64)1 << p;
                supp[i][j][c1] = doms[j] & ~incompat;
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
                    u64 to_rem = 0, tmp = doms[i];
                    while (tmp) {
                        int c1 = ctz64(tmp); tmp &= tmp - 1;
                        u64 c2m = supp[i][j][c1], c3c1 = supp[i][kk][c1];
                        if (!c3c1) { to_rem |= (u64)1 << c1; continue; }
                        bool has_ext = false;
                        u64 cm = c2m;
                        while (cm && !has_ext) {
                            int c2 = ctz64(cm); cm &= cm - 1;
                            if (supp[j][kk][c2] & c3c1) has_ext = true;
                        }
                        if (!has_ext) to_rem |= (u64)1 << c1;
                    }
                    if (to_rem) {
                        changed = true;
                        doms[i] &= ~to_rem;
                        if (!doms[i]) return true;
                        for (int m = 0; m < n; m++) {
                            if (m == i) continue;
                            u64 t2 = to_rem;
                            while (t2) { int c1 = ctz64(t2); t2 &= t2-1; supp[i][m][c1] = 0; }
                            u64 t3 = doms[m];
                            while (t3) { int cm = ctz64(t3); t3 &= t3-1; supp[m][i][cm] &= doms[i]; }
                        }
                    }
                }
            }
        }
    }
    return false;
}

// ─── BIPARTITE HALL ──────────────────────────────────────────────────────────

static int bip_mc[MAXN];

static bool bip_aug(int i, const u64* doms, int N, bool* vis) {
    u64 d = doms[i];
    while (d) {
        int c = ctz64(d); d &= d - 1;
        if (vis[c]) continue;
        vis[c] = true;
        if (bip_mc[c] < 0 || bip_aug(bip_mc[c], doms, N, vis)) {
            bip_mc[c] = i; return true;
        }
    }
    return false;
}

static bool bipartite_hall(const u64* doms, int ns, int N) {
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

static bool bip_aug_ud(int i, const u64* doms, const int* rows, int N, bool* vis) {
    u64 d = doms[i];
    while (d) {
        int c = ctz64(d); d &= d - 1;
        int ud = rows[i] + c;
        if (vis[ud]) continue;
        vis[ud] = true;
        if (bip_mc_ud[ud] < 0 || bip_aug_ud(bip_mc_ud[ud], doms, rows, N, vis)) {
            bip_mc_ud[ud] = i; return true;
        }
    }
    return false;
}

static bool bip_aug_dd(int i, const u64* doms, const int* rows, int N, bool* vis) {
    u64 d = doms[i];
    while (d) {
        int c = ctz64(d); d &= d - 1;
        int dd = (c - rows[i]) + (N - 1);
        if (vis[dd]) continue;
        vis[dd] = true;
        if (bip_mc_dd[dd] < 0 || bip_aug_dd(bip_mc_dd[dd], doms, rows, N, vis)) {
            bip_mc_dd[dd] = i; return true;
        }
    }
    return false;
}

static bool hall_updiag(const u64* doms, const int* rows, int ns, int N) {
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

static bool hall_downdiag(const u64* doms, const int* rows, int ns, int N) {
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

static bool regan_col(u64* doms, const int* rows, int ns, int N) {
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
        u64 tmp = doms[i]; while (tmp) {
            int c = ctz64(tmp); tmp &= tmp - 1;
            if (c != mc) ts.add_edge(i, ns + c);
        }
    }
    ts.run(total);
    for (int i = 0; i < ns; i++) {
        int mc = match_row[i];
        u64 to_rem = 0, tmp = doms[i]; while (tmp) {
            int c = ctz64(tmp); tmp &= tmp - 1;
            if (c != mc && ts.scc[i] != ts.scc[ns + c]) to_rem |= (u64)1 << c;
        }
        if (to_rem) { doms[i] &= ~to_rem; if (!doms[i]) return true; }
    }
    return false;
}

// ─── PROPAGACIÓN COMPLETA ────────────────────────────────────────────────────

static bool propagate_all(u64* doms, const int* rows, int ns, int N) {
    for (int iter = 0; iter < 8; iter++) {
        u64 saved[MAXN]; memcpy(saved, doms, ns * sizeof(u64));
#define CHK(name, call) do { if (call) { if(g_debug_propagate) fprintf(stderr,"UNSAT at %s iter=%d\n",name,iter); return true; } } while(0)
        CHK("ac3_1",    ac3_bits(doms, rows, ns, N));
        CHK("hall_col", bipartite_hall(doms, ns, N));
        CHK("hall_up",  hall_updiag(doms, rows, ns, N));
        CHK("hall_dn",  hall_downdiag(doms, rows, ns, N));
        CHK("regan_col",regan_col(doms, rows, ns, N));
        CHK("ac3_2",    ac3_bits(doms, rows, ns, N));
        // regan_updiag / regan_downdiag disabled: soundness bug
        bool stable = true;
        for (int i = 0; i < ns; i++) if (doms[i] != saved[i]) { stable = false; break; }
        if (stable) return false;
    }
    return false;
}

// ─── TRIPLE CONSISTENCY ──────────────────────────────────────────────────────

static bool triple_unsat(const u64* doms, const int* rows, int ns, int N) {
    for (int i = 0; i < ns - 2; i++)
        for (int j = i + 1; j < ns - 1; j++) {
            int dij = abs(rows[i] - rows[j]);
            for (int k = j + 1; k < ns; k++) {
                int dik = abs(rows[i] - rows[k]);
                int djk = abs(rows[j] - rows[k]);
                bool found = false;
                u64 tmp_i = doms[i];
                while (tmp_i && !found) {
                    int ci = ctz64(tmp_i); tmp_i &= tmp_i - 1;
                    u64 cj_avail = doms[j] & ~((u64)1 << ci);
                    int p = ci + dij; if (p < N)  cj_avail &= ~((u64)1 << p);
                    p     = ci - dij; if (p >= 0) cj_avail &= ~((u64)1 << p);
                    if (!cj_avail) continue;
                    u64 ck_ci = doms[k] & ~((u64)1 << ci);
                    p = ci + dik; if (p < N)  ck_ci &= ~((u64)1 << p);
                    p = ci - dik; if (p >= 0) ck_ci &= ~((u64)1 << p);
                    u64 tmp_j = cj_avail;
                    while (tmp_j && !found) {
                        int cj = ctz64(tmp_j); tmp_j &= tmp_j - 1;
                        u64 ck = ck_ci & ~((u64)1 << cj);
                        p = cj + djk; if (p < N)  ck &= ~((u64)1 << p);
                        p = cj - djk; if (p >= 0) ck &= ~((u64)1 << p);
                        if (ck) found = true;
                    }
                }
                if (!found) return true;
            }
        }
    return false;
}
