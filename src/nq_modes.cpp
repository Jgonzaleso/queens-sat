// nq_modes.cpp — búsqueda, análisis y modos auxiliares.
// Requiere nq_propagate.cpp + nq_pipeline.cpp incluidos antes.

#include <vector>
#include <functional>
#include <map>
#include <numeric>

// ─── NO-GOODS (clause learning) ──────────────────────────────────────────────

struct NoGood {
    int n;
    int8_t rows[MAXN], cols[MAXN];
};

static std::vector<NoGood> s_nogoods;
static bool s_ng_enabled = false;

static void add_nogood(int depth) {
    if (!s_ng_enabled || depth == 0 || (int)s_nogoods.size() >= 200000) return;
    NoGood ng; ng.n = depth;
    for (int i = 0; i < depth; i++) {
        ng.rows[i] = (int8_t)g_qr[i];
        ng.cols[i] = (int8_t)g_qc[i];
    }
    s_nogoods.push_back(ng);
}

static bool check_nogood(int depth, int new_row, int new_col) {
    for (const auto& ng : s_nogoods) {
        if (ng.n != depth + 1) continue;
        int matched = 0;
        for (int i = 0; i < ng.n; i++) {
            int r = ng.rows[i], c = ng.cols[i];
            if (r == new_row && c == new_col) { matched++; continue; }
            for (int j = 0; j < depth; j++)
                if (g_qr[j] == r && g_qc[j] == c) { matched++; break; }
        }
        if (matched == ng.n) return true;
    }
    return false;
}

// ─── BT ORACLE (solo para modos legacy: search_nodet, collect) ───────────────

static int bt_rows[MAXN], bt_nr_g;
static bitmask bt_doms_g[MAXN];

static bool bt_rec(int idx, int N) {
    if (idx == bt_nr_g) return true;
    int r = bt_rows[idx];
    bitmask avail = bt_doms_g[idx];
    while (avail) {
        int c = ctz_bm(avail); avail &= avail-1;
        int rem = bt_nr_g - idx - 1;
        bitmask saved[MAXN];
        if (rem > 0) memcpy(saved, bt_doms_g+idx+1, rem*sizeof(bitmask));
        bool ok = true;
        for (int j=idx+1; j<bt_nr_g; j++) {
            bt_doms_g[j] &= ~attack_mask(r, c, bt_rows[j], N);
            if (!bt_doms_g[j]) { ok=false; break; }
        }
        if (ok && bt_rec(idx+1, N)) return true;
        if (rem > 0) memcpy(bt_doms_g+idx+1, saved, rem*sizeof(bitmask));
    }
    return false;
}

bool bt_oracle(const int* qr, const int* qc, int K, int N) {
    bool used[MAXN]={};
    for (int i=0;i<K;i++) used[qr[i]]=true;
    bt_nr_g = 0;
    for (int r=0;r<N;r++) if (!used[r]) bt_rows[bt_nr_g++]=r;
    for (int i=0;i<bt_nr_g;i++) {
        bt_doms_g[i] = available_bits(bt_rows[i], N, qr, qc, K);
        if (!bt_doms_g[i]) return false;
    }
    if (ac3_bits(bt_doms_g, bt_rows, bt_nr_g, N)) return false;
    return bt_rec(0, N);
}

// ─── GENERADOR SIN GARANTÍA SAT ──────────────────────────────────────────────

bool gen_placement_raw(int N, int K, int* qr, int* qc, std::mt19937& rng) {
    for (int attempt=0; attempt<1000; attempt++) {
        int rows[MAXN]; for (int i=0;i<N;i++) rows[i]=i;
        std::shuffle(rows, rows+N, rng);
        bool ok=true; int placed=0;
        bool uc[MAXN]={}, ud1[2*MAXN]={}, ud2[2*MAXN]={};
        for (int ri=0;ri<K;ri++) {
            int r=rows[ri];
            int avail[MAXN]; int na=0;
            for (int c=0;c<N;c++)
                if (!uc[c]&&!ud1[c-r+N]&&!ud2[c+r]) avail[na++]=c;
            if (na==0) { ok=false; break; }
            int c=avail[std::uniform_int_distribution<int>(0,na-1)(rng)];
            qr[placed]=r; qc[placed]=c; placed++;
            uc[c]=true; ud1[c-r+N]=true; ud2[c+r]=true;
        }
        if (ok) return true;
    }
    return false;
}

// ─── ANÁLISIS K-CONSISTENCIA ─────────────────────────────────────────────────

static int kc_vals[MAXN];

bool has_valid_k(const bitmask* doms, const int* singles, const int* sub, int k, int depth, int N) {
    if (depth == k) return true;
    int idx = sub[depth];
    int r2  = singles[idx];
    bitmask tmp = doms[idx];
    while (tmp) {
        int c = ctz_bm(tmp); tmp &= tmp-1;
        bool ok = true;
        for (int prev=0; prev<depth && ok; prev++) {
            int r1 = singles[sub[prev]];
            int diff = abs(r1-r2);
            if (kc_vals[prev]==c || abs(kc_vals[prev]-c)==diff) ok=false;
        }
        if (!ok) continue;
        kc_vals[depth] = c;
        if (has_valid_k(doms, singles, sub, k, depth+1, N)) return true;
    }
    return false;
}

int find_min_incons(const bitmask* doms, const int* singles, int ns, int k_max, int N, int* sub_out) {
    for (int k=2; k<=k_max; k++) {
        int sub[MAXN]; for (int i=0;i<k;i++) sub[i]=i;
        while (true) {
            if (!has_valid_k(doms, singles, sub, k, 0, N)) {
                memcpy(sub_out, sub, k*sizeof(int));
                return k;
            }
            int i = k-1;
            while (i>=0 && sub[i]==ns-k+i) i--;
            if (i<0) break;
            sub[i]++;
            for (int j=i+1;j<k;j++) sub[j]=sub[j-1]+1;
        }
    }
    return 0;
}

// ─── ANÁLISIS Y PRINT DE INSTANCIA NO_DET ────────────────────────────────────

void analyze_nodet(const int* qr, const int* qc, int K, int N, int k_max) {
    bool used[MAXN]={};
    for (int i=0;i<K;i++) used[qr[i]]=true;
    int singles[MAXN]; int ns=0;
    for (int r=0;r<N;r++) if (!used[r]) singles[ns++]=r;

    printf("  Queens (%d): ", K);
    for (int i=0;i<K;i++) printf("(%d,%d) ", qr[i], qc[i]);
    printf("\n");
    printf("  Singles (%d):\n", ns);

    bitmask doms[MAXN];
    for (int i=0;i<ns;i++) doms[i] = available_bits(singles[i], N, qr, qc, K);

    int min_sz=N+1, max_sz=0; float avg_sz=0;
    for (int i=0;i<ns;i++) {
        int sz=pop_bm(doms[i]);
        min_sz=std::min(min_sz,sz); max_sz=std::max(max_sz,sz); avg_sz+=sz;
        printf("    fila %2d: sz=%2d  [", singles[i], sz);
        bitmask tmp=doms[i]; int cnt=0;
        while (tmp && cnt<8) { int c=ctz_bm(tmp); tmp&=tmp-1; printf("%d,",c); cnt++; }
        if (tmp) printf("...");
        printf("]\n");
    }
    avg_sz /= ns;
    printf("  Domain sizes: min=%d max=%d avg=%.1f\n", min_sz, max_sz, avg_sz);

    bitmask a3[MAXN]; memcpy(a3, doms, ns*sizeof(bitmask));
    bool ac3_det = ac3_bits(a3, singles, ns, N);
    printf("  AC-3: %s\n", ac3_det ? "UNSAT v" : "NO_DET");

    bitmask sac_d[MAXN]; memcpy(sac_d, doms, ns*sizeof(bitmask));
    bool sac_det = sac_bits(sac_d, singles, ns, N);
    printf("  SAC:  %s", sac_det ? "UNSAT v" : "NO_DET");
    if (!sac_det) {
        int red=0;
        for (int i=0;i<ns;i++) if (pop_bm(doms[i]) != pop_bm(sac_d[i])) red++;
        printf("  (redujo %d filas)", red);
    }
    printf("\n");

    bitmask doms_ac[MAXN]; memcpy(doms_ac, doms, ns*sizeof(bitmask));
    ac3_bits(doms_ac, singles, ns, N);

    int sub_out[MAXN];
    int k_found = find_min_incons(doms_ac, singles, ns, k_max, N, sub_out);
    if (k_found == 0) {
        printf("  K-consistencia: consistente hasta k=%d\n", k_max);
    } else {
        printf("  K-inconsistencia detectada en k=%d: filas [", k_found);
        for (int i=0;i<k_found;i++) printf("%d%s", singles[sub_out[i]], i<k_found-1?",":"");
        printf("]\n");
        printf("    Dominios del subconjunto:\n");
        for (int i=0;i<k_found;i++) {
            int idx=sub_out[i];
            printf("      fila %2d: [", singles[idx]);
            bitmask tmp=doms_ac[idx]; int cnt=0;
            while (tmp && cnt<12) { int c=ctz_bm(tmp); tmp&=tmp-1; printf("%d,",c); cnt++; }
            printf("] sz=%d\n", pop_bm(doms_ac[idx]));
        }
    }
    printf("\n");
}

// ─── CNF ENCODER ─────────────────────────────────────────────────────────────

void write_completion_cnf(const char* fname, int N, const int* qr, const int* qc, int K) {
    bool placed_row[MAXN]={}, placed_col[MAXN]={};
    for (int i=0;i<K;i++) { placed_row[qr[i]]=true; placed_col[qc[i]]=true; }

    auto var = [&](int r, int c) -> int { return r*N + c + 1; };

    int n_clauses = 0;
    for (int i=0;i<K;i++) {
        n_clauses++;
        for (int c=0;c<N;c++) if (c!=qc[i]) n_clauses++;
        for (int r=0;r<N;r++) if (r!=qr[i]) n_clauses++;
        for (int d=1;d<N;d++) {
            if (qr[i]+d<N&&qc[i]+d<N) n_clauses++;
            if (qr[i]+d<N&&qc[i]-d>=0) n_clauses++;
            if (qr[i]-d>=0&&qc[i]+d<N) n_clauses++;
            if (qr[i]-d>=0&&qc[i]-d>=0) n_clauses++;
        }
    }
    for (int r=0;r<N;r++) if (!placed_row[r]) n_clauses++;
    for (int c=0;c<N;c++) {
        std::vector<int> fr;
        for (int r=0;r<N;r++) if (!placed_row[r]) fr.push_back(r);
        n_clauses += (int)(fr.size()*(fr.size()-1)/2);
    }
    for (int d=0;d<2*N-1;d++) {
        std::vector<std::pair<int,int>> cells;
        for (int r=0;r<N;r++) { int c=d-r; if(c>=0&&c<N&&!placed_row[r]) cells.push_back({r,c}); }
        n_clauses += (int)(cells.size()*(cells.size()-1)/2);
    }
    for (int d=-(N-1);d<N;d++) {
        std::vector<std::pair<int,int>> cells;
        for (int r=0;r<N;r++) { int c=r-d; if(c>=0&&c<N&&!placed_row[r]) cells.push_back({r,c}); }
        n_clauses += (int)(cells.size()*(cells.size()-1)/2);
    }

    FILE* f = fopen(fname, "w");
    if (!f) { printf("ERROR: no se pudo abrir %s\n", fname); return; }
    fprintf(f, "p cnf %d %d\n", N*N, n_clauses);

    for (int i=0;i<K;i++) {
        fprintf(f, "%d 0\n", var(qr[i],qc[i]));
        for (int c=0;c<N;c++) if (c!=qc[i]) fprintf(f, "-%d 0\n", var(qr[i],c));
        for (int r=0;r<N;r++) if (r!=qr[i]) fprintf(f, "-%d 0\n", var(r,qc[i]));
        for (int d=1;d<N;d++) {
            if (qr[i]+d<N&&qc[i]+d<N) fprintf(f,"-%d 0\n",var(qr[i]+d,qc[i]+d));
            if (qr[i]+d<N&&qc[i]-d>=0) fprintf(f,"-%d 0\n",var(qr[i]+d,qc[i]-d));
            if (qr[i]-d>=0&&qc[i]+d<N) fprintf(f,"-%d 0\n",var(qr[i]-d,qc[i]+d));
            if (qr[i]-d>=0&&qc[i]-d>=0) fprintf(f,"-%d 0\n",var(qr[i]-d,qc[i]-d));
        }
    }
    for (int r=0;r<N;r++) {
        if (placed_row[r]) continue;
        for (int c=0;c<N;c++) fprintf(f, "%d ", var(r,c));
        fprintf(f, "0\n");
    }
    for (int c=0;c<N;c++) {
        std::vector<int> fr;
        for (int r=0;r<N;r++) if (!placed_row[r]) fr.push_back(r);
        for (int a=0;a<(int)fr.size();a++)
            for (int b=a+1;b<(int)fr.size();b++)
                fprintf(f, "-%d -%d 0\n", var(fr[a],c), var(fr[b],c));
    }
    for (int d=0;d<2*N-1;d++) {
        std::vector<std::pair<int,int>> cells;
        for (int r=0;r<N;r++) { int c=d-r; if(c>=0&&c<N&&!placed_row[r]) cells.push_back({r,c}); }
        for (int a=0;a<(int)cells.size();a++)
            for (int b=a+1;b<(int)cells.size();b++)
                fprintf(f,"-%d -%d 0\n",var(cells[a].first,cells[a].second),
                        var(cells[b].first,cells[b].second));
    }
    for (int d=-(N-1);d<N;d++) {
        std::vector<std::pair<int,int>> cells;
        for (int r=0;r<N;r++) { int c=r-d; if(c>=0&&c<N&&!placed_row[r]) cells.push_back({r,c}); }
        for (int a=0;a<(int)cells.size();a++)
            for (int b=a+1;b<(int)cells.size();b++)
                fprintf(f,"-%d -%d 0\n",var(cells[a].first,cells[a].second),
                        var(cells[b].first,cells[b].second));
    }
    fclose(f);
}

// ─── GREEDY COMPLETO CON BT Y NO-GOODS ───────────────────────────────────────

bool greedy_rec(int k, int N, long long& bt, long long bt_limit) {
    bool used[MAXN]={};
    for (int i=0;i<k;i++) used[g_qr[i]]=true;
    int singles[MAXN]; int ns=0;
    for (int r=0;r<N;r++) if (!used[r]) singles[ns++]=r;
    if (ns==0) return true;

    int best_row=-1, best_sz=N+1; bitmask best_mask=0;
    for (int i=0;i<ns;i++) {
        bitmask m = available_bits(singles[i], N, g_qr, g_qc, k);
        int sz = pop_bm(m);
        if (sz==0) return false;
        if (sz<best_sz) { best_sz=sz; best_row=singles[i]; best_mask=m; }
    }

    bitmask tmp = best_mask;
    while (tmp) {
        if (bt >= bt_limit) return false;
        int c = ctz_bm(tmp); tmp &= tmp-1;
        if (s_ng_enabled && check_nogood(k, best_row, c)) continue;
        g_qr[k]=best_row; g_qc[k]=c;
        PResult pr = pipeline(g_qr, g_qc, k+1, N);
        if (pr==UNSAT_DET) continue;
        if (greedy_rec(k+1, N, bt, bt_limit)) return true;
        bt++;
    }
    add_nogood(k);
    return false;
}

// ─── BÚSQUEDA PIPELINE AUTÓNOMO (sin BT oracle) ──────────────────────────────

void search_pipeline_only(int N, int K, long long n_target, int seed) {
    printf("================================================================\n");
    printf("  PIPELINE AUTONOMO (sin BT): N=%d  K=%d  intentos=%lld  seed=%d\n",
           N, K, n_target, seed);
    printf("================================================================\n");
    fflush(stdout);

    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN], sol[MAXN];
    long long n_att=0, n_unsat=0, n_sat_ok=0, n_sat_err=0, n_incomplete=0;

    auto t0 = std::chrono::steady_clock::now();
    long long report_step = std::max(n_target/10, (long long)10000);

    while (n_att < n_target) {
        n_att++;
        if (!gen_placement_raw(N, K, qr, qc, rng)) continue;

        PResult res = pipeline_solve(qr, qc, K, N, sol);

        if (res == UNSAT_DET) {
            n_unsat++;
        } else {
            bool complete = true;
            for (int r=0;r<N&&complete;r++) if (sol[r]<0) complete=false;
            if (!complete) { n_incomplete++; continue; }
            if (verify_solution(sol, N)) n_sat_ok++;
            else n_sat_err++;
        }

        if (n_att % report_step == 0) {
            auto t1 = std::chrono::steady_clock::now();
            float sec = std::chrono::duration<float>(t1-t0).count();
            long long proc = n_unsat+n_sat_ok+n_sat_err+n_incomplete;
            printf("  att=%lld  UNSAT=%lld(%.4f%%)  SAT_ok=%lld  fail=%lld  %.1fs\n",
                   n_att, n_unsat, 100.0*n_unsat/std::max(proc,1LL),
                   n_sat_ok, n_incomplete, sec);
            fflush(stdout);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    float sec = std::chrono::duration<float>(t1-t0).count();
    long long proc = n_unsat+n_sat_ok+n_sat_err+n_incomplete;
    printf("\n================================================================\n");
    printf("  RESUMEN FINAL\n");
    printf("================================================================\n");
    printf("  Intentos    : %lld\n", n_att);
    printf("  UNSAT_DET   : %lld  (%.4f%% de procesados, sin BT)\n",
           n_unsat, 100.0*n_unsat/std::max(proc,1LL));
    printf("  SAT+valido  : %lld  (%.4f%%, solucion verificada O(N^2))\n",
           n_sat_ok, 100.0*n_sat_ok/std::max(proc,1LL));
    printf("  greedy_fail : %lld  (%.4f%%, SAT_POSSIBLE sin testigo)\n",
           n_incomplete, 100.0*n_incomplete/std::max(proc,1LL));
    if (n_sat_err) printf("  SAT_err     : %lld  *** BUG ***\n", n_sat_err);
    printf("  Pipeline resuelve: %.4f%% de instancias completamente\n",
           100.0*(n_unsat+n_sat_ok)/std::max(proc,1LL));
    printf("  Tiempo      : %.1fs  (%.2f us/instancia)\n",
           sec, 1e6*sec/std::max(n_att,1LL));
    printf("================================================================\n");
}

// ─── AVG_PEER: scoring de estructura de celdas viables ───────────────────────
// avg_peer = fraccion media de otras celdas viables compatibles por celda viable.
// UNSAT en K_min tiene avg_peer sistematicamente menor que SAT (98-100% a N>=17).
//
// Formula O(NF^2) via algebra de bitmask s:
//   total_peers = (TV^2 - sum|dom[i]|^2) - conflicts
//   conflicts   = sum_{i<j} 2*(pop(dom[i] & dom[j])          // misma col
//                             + pop(dom[i] & (dom[j] >> d))  // diagonal +
//                             + pop(dom[i] & (dom[j] << d))) // diagonal -
//   d = |row_i - row_j|
// Esto reemplaza el loop O(TV^2) por O(NF^2) ops de bitmask.

double compute_avg_peer(const int* qr, const int* qc, int K, int N) {
    bool placed[MAXN] = {};
    for (int i = 0; i < K; i++) placed[qr[i]] = true;

    int free_rows[MAXN]; int NF = 0;
    bitmask dom[MAXN];
    for (int r = 0; r < N; r++) {
        if (!placed[r]) {
            dom[NF] = available_bits(r, N, qr, qc, K);
            free_rows[NF++] = r;
        }
    }
    if (NF < 2) return 0.0;

    // TV y suma de cuadrados
    long long TV = 0, sq_sum = 0;
    for (int i = 0; i < NF; i++) {
        long long p = pop_bm(dom[i]);
        TV     += p;
        sq_sum += p * p;
    }
    if (TV < 2) return 0.0;

    // Conflictos via bitmask: O(NF^2)
    long long conflicts = 0;
    for (int i = 0; i < NF; i++) {
        for (int j = i + 1; j < NF; j++) {
            int d = free_rows[j] - free_rows[i];  // siempre > 0 (filas ordenadas)
            long long cc = pop_bm(dom[i] & dom[j]);
            long long dc = pop_bm(dom[i] & (dom[j] >> d))
                         + pop_bm(dom[i] & (dom[j] << d));
            conflicts += 2 * (cc + dc);
        }
    }

    long long total_peers = (TV * TV - sq_sum) - conflicts;
    return (double)total_peers / TV / (TV - 1);
}

// ─── SHRINK: reduce UNSAT de K_start a K_target (solo pipeline) ──────────────

void search_shrink(int N, int K_start, int K_target, long long n_att, int seed) {
    printf("================================================================\n");
    printf("  SHRINK: N=%d  K_start=%d -> K_target=%d  intentos=%lld  seed=%d\n",
           N, K_start, K_target, n_att, seed);
    printf("  max_combos=%d  depth=%d\n", g_max_combos, g_pipeline_depth);
    printf("================================================================\n");
    fflush(stdout);

    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];

    long long n_gen=0, n_unsat_start=0, n_reduced=0, n_detected=0, n_escaped=0;
    long long bud_hist[7]={};
    const char* bud_labels[]={"prop.","<=8","<=25","<=100","<=400","<=2000","NUNCA"};

    struct EscapeInst { int qr[MAXN], qc[MAXN], K; };
    EscapeInst escaped[5]; int n_esc_saved=0;

    auto t0 = std::chrono::steady_clock::now();

    while (n_gen < n_att) {
        n_gen++;
        if (!gen_placement_raw(N, K_start, qr, qc, rng)) continue;

        if (pipeline(qr, qc, K_start, N) != UNSAT_DET) continue;
        n_unsat_start++;

        int cur_qr[MAXN], cur_qc[MAXN];
        memcpy(cur_qr, qr, K_start*sizeof(int));
        memcpy(cur_qc, qc, K_start*sizeof(int));
        int cur_K = K_start;

        while (cur_K > K_target) {
            bool removed = false;
            int order[MAXN]; for(int i=0;i<cur_K;i++) order[i]=i;
            std::shuffle(order, order+cur_K, rng);
            for (int oi=0; oi<cur_K; oi++) {
                int idx = order[oi];
                int nqr[MAXN], nqc[MAXN]; int nk=0;
                for (int j=0;j<cur_K;j++) if (j!=idx) { nqr[nk]=cur_qr[j]; nqc[nk]=cur_qc[j]; nk++; }
                if (pipeline(nqr, nqc, nk, N) == UNSAT_DET) {
                    memcpy(cur_qr, nqr, nk*sizeof(int));
                    memcpy(cur_qc, nqc, nk*sizeof(int));
                    cur_K = nk; removed = true; break;
                }
            }
            if (!removed) break;
        }

        if (cur_K > K_target) continue;
        n_reduced++;

        // Imprimir posicion para analisis externo (parseada por Python)
        printf("  INST_K%d:", cur_K);
        for(int i=0;i<cur_K;i++) printf(" (%d,%d)",cur_qr[i],cur_qc[i]);
        printf("\n"); fflush(stdout);

        CMRResult cmr = cmr_analyze(cur_qr, cur_qc, cur_K, N);
        int b;
        if      (cmr.detect_budget == 0)    b=0;
        else if (cmr.detect_budget <=   8)  b=1;
        else if (cmr.detect_budget <=  25)  b=2;
        else if (cmr.detect_budget <= 100)  b=3;
        else if (cmr.detect_budget <= 400)  b=4;
        else if (cmr.detect_budget <= 2000) b=5;
        else                                b=6;
        bud_hist[b]++;

        if (b >= 4) {
            // Imprimir análisis de dominio para instancias duras (≥400) y escapadas
            const char* bname = (b==4)?"<=400":(b==5)?"<=2000":"NUNCA";
            if (b==6) { n_escaped++; printf("\n  *** ESCAPADA #%lld ***\n", n_escaped); }
            else       printf("\n  [DURA #%lld  CMR=%s]\n", n_reduced, bname);
            printf("  K=%d->%d  ns=%d  min_dom=%d  avg_dom=%.1f  detect_bud=%d\n",
                   K_start, cur_K, cmr.ns, cmr.min_dom,
                   cmr.avg_dom_x10/10.0, cmr.detect_budget);
            // Dominios de las filas libres post-propagación
            {
                bool used[MAXN]={}; for(int i=0;i<cur_K;i++) used[cur_qr[i]]=true;
                int singles[MAXN]; int ns2=0; bitmask d0[MAXN];
                for(int r=0;r<N;r++) if(!used[r]) { d0[ns2]=available_bits(r,N,cur_qr,cur_qc,cur_K); singles[ns2++]=r; }
                bitmask d1[MAXN]; memcpy(d1,d0,ns2*sizeof(bitmask)); propagate_all(d1,singles,ns2,N);
                // Ordenar por dominio
                int ord[MAXN]; for(int i=0;i<ns2;i++) ord[i]=i;
                std::sort(ord,ord+ns2,[&](int a,int b2){return pop_bm(d1[a])<pop_bm(d1[b2]);});
                printf("  Dominios (menores primero):");
                for(int i=0;i<std::min(ns2,10);i++) printf(" %d", pop_bm(d1[ord[i]]));
                if(ns2>10) printf(" ...");
                printf("\n");
                // Producto acumulado (cuántos pivotes elije choose_P a bud=2000)
                long long prod=1; int P=0;
                for(int i=0;i<ns2&&P<10;i++) {
                    long long np=prod*(long long)pop_bm(d1[ord[i]]);
                    if(np>2000) break;
                    prod=np; P++;
                }
                printf("  choose_P(bud=2000): P=%d  product=%lld\n", P, prod);
            }
            printf("  Reinas:");
            for(int i=0;i<cur_K;i++) printf(" (%d,%d)",cur_qr[i],cur_qc[i]);
            printf("\n");
            if (b==6 && n_esc_saved < 5) {
                char fname[256];
                snprintf(fname,sizeof(fname),"escaped_%dx%d_%d.cnf",N,cur_K,n_esc_saved+1);
                write_completion_cnf(fname, N, cur_qr, cur_qc, cur_K);
                printf("  [CNF: %s]\n", fname);
                n_esc_saved++;
            }
            fflush(stdout);
        }
        if (b < 6) n_detected++;

        if (n_reduced % 20 == 0 || n_reduced <= 5) {
            float sec=std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
            printf("  gen=%lld unsat=%lld red=%lld det=%lld esc=%lld  %.1fs\n",
                   n_gen, n_unsat_start, n_reduced, n_detected, n_escaped, sec);
            fflush(stdout);
        }
    }

    float sec=std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();

    printf("\n================================================================\n");
    printf("  SHRINK RESULTADO FINAL\n");
    printf("================================================================\n");
    printf("  Intentos totales   : %lld\n", n_gen);
    printf("  UNSAT con K=%d    : %lld\n", K_start, n_unsat_start);
    printf("  Reducidos a K<=%d  : %lld (%.1f%% de UNSAT)\n",
           K_target, n_reduced, n_unsat_start?100.0*n_reduced/n_unsat_start:0);
    if (n_reduced > 0) {
        printf("\n  CMR de instancias reducidas:\n");
        for (int b=0;b<7;b++)
            if (bud_hist[b]) printf("    %s: %lld (%.1f%%)\n",
                bud_labels[b], bud_hist[b], 100.0*bud_hist[b]/n_reduced);
        printf("\n  Detectadas por pipeline: %lld (%.1f%%)\n",
               n_detected, 100.0*n_detected/n_reduced);
        printf("  ESCAPADAS (puntos ciegos): %lld (%.1f%%)\n",
               n_escaped, 100.0*n_escaped/n_reduced);
        if (n_escaped == 0) printf("  -> Pipeline detecta TODO hasta K=%d via reduccion\n", K_target);
        else                printf("  -> INSTANCIAS QUE SUPERAN PIPELINE: ver escaped_*.cnf\n");
    }
    printf("  Tiempo: %.1fs\n", sec);
    printf("================================================================\n");
}

// ─── SHRINK_GUIDED: shrink guiado por avg_peer (minimizar) ───────────────────
// En cada paso de reduccion, prueba TODOS los K retiros posibles que mantienen
// UNSAT_DET y elige el que minimiza avg_peer -> estructura mas cercana a K_min UNSAT.
// Empirico: avg_peer elimina 98-100% de SAT en N>=17.

// n_restarts: caminos de reduccion por UNSAT encontrado en K_start.
//   restart 0 = greedy puro (min avg_peer en cada paso)
//   restart 1+ = exploracion: elige aleatoriamente entre top-2 retiros validos
// Esto explora distintos caminos sin repetir la costosa generacion inicial.
void search_shrink_guided(int N, int K_start, int K_target, long long n_att,
                          int seed, int n_restarts = 1) {
    printf("================================================================\n");
    printf("  SHRINK_GUIDED: N=%d  K_start=%d -> K_target=%d\n",
           N, K_start, K_target);
    printf("  intentos=%lld  seed=%d  restarts=%d  score=min(avg_peer)\n",
           n_att, seed, n_restarts);
    printf("  max_combos=%d  depth=%d\n", g_max_combos, g_pipeline_depth);
    printf("================================================================\n");
    fflush(stdout);

    std::mt19937 gen_rng(seed);          // solo para gen_placement_raw
    std::mt19937 exp_rng(seed ^ 0xDEAD); // para seleccion estocastica
    int qr[MAXN], qc[MAXN];

    long long n_gen=0, n_unsat_start=0, n_reduced=0, n_detected=0, n_escaped=0;
    long long bud_hist[7]={};
    auto t0 = std::chrono::steady_clock::now();

    while (n_gen < n_att) {
        n_gen++;
        if (!gen_placement_raw(N, K_start, qr, qc, gen_rng)) continue;
        if (pipeline(qr, qc, K_start, N) != UNSAT_DET) continue;
        n_unsat_start++;

        // Multi-restart: probar n_restarts caminos desde el mismo UNSAT inicial
        bool found_this = false;
        for (int restart = 0; restart < n_restarts && !found_this; restart++) {
            int cur_qr[MAXN], cur_qc[MAXN];
            memcpy(cur_qr, qr, K_start*sizeof(int));
            memcpy(cur_qc, qc, K_start*sizeof(int));
            int cur_K = K_start;

            // Guided greedy con exploracion en restarts > 0
            while (cur_K > K_target) {
                struct Cand { double peer; int idx; };
                Cand cands[MAXN]; int nc = 0;

                for (int idx = 0; idx < cur_K; idx++) {
                    int nqr[MAXN], nqc[MAXN]; int nk = 0;
                    for (int j = 0; j < cur_K; j++)
                        if (j != idx) { nqr[nk]=cur_qr[j]; nqc[nk]=cur_qc[j]; nk++; }
                    if (pipeline(nqr, nqc, nk, N) != UNSAT_DET) continue;
                    double peer = compute_avg_peer(nqr, nqc, nk, N);
                    cands[nc++] = {peer, idx};
                }

                if (nc == 0) break;

                // Ordenar por peer ascendente (si hay mas de 1 candidato)
                if (nc > 1)
                    std::sort(cands, cands+nc,
                              [](const Cand& a, const Cand& b){ return a.peer < b.peer; });

                // Seleccion: restart 0 = greedy puro (rank 0),
                //            restart 1+ = aleatorio entre top-min(2,nc) via exp_rng
                int pick = 0;
                if (restart > 0 && nc > 1)
                    pick = exp_rng() % std::min(nc, 2);

                int best_idx = cands[pick].idx;
                int nqr[MAXN], nqc[MAXN]; int nk = 0;
                for (int j = 0; j < cur_K; j++)
                    if (j != best_idx) { nqr[nk]=cur_qr[j]; nqc[nk]=cur_qc[j]; nk++; }
                memcpy(cur_qr, nqr, nk*sizeof(int));
                memcpy(cur_qc, nqc, nk*sizeof(int));
                cur_K = nk;
            }

            if (cur_K > K_target) continue;
            found_this = true;
            n_reduced++;

        printf("  INST_K%d:", cur_K);
        for (int i = 0; i < cur_K; i++) printf(" (%d,%d)", cur_qr[i], cur_qc[i]);
        printf("\n"); fflush(stdout);

        CMRResult cmr = cmr_analyze(cur_qr, cur_qc, cur_K, N);
        int b;
        if      (cmr.detect_budget == 0)    b=0;
        else if (cmr.detect_budget <=   8)  b=1;
        else if (cmr.detect_budget <=  25)  b=2;
        else if (cmr.detect_budget <= 100)  b=3;
        else if (cmr.detect_budget <= 400)  b=4;
        else if (cmr.detect_budget <= 2000) b=5;
        else                                b=6;
        bud_hist[b]++;
        if (b < 6) n_detected++;
        else       n_escaped++;

        if (n_reduced % 10 == 0 || n_reduced <= 5) {
            float sec = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
            printf("  gen=%lld unsat=%lld red=%lld det=%lld esc=%lld  %.1fs\n",
                   n_gen, n_unsat_start, n_reduced, n_detected, n_escaped, sec);
            fflush(stdout);
        }
        }  // end for restart
    }  // end while n_gen

    float sec = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
    printf("\n================================================================\n");
    printf("  SHRINK_GUIDED RESULTADO FINAL\n");
    printf("================================================================\n");
    printf("  Intentos totales  : %lld\n", n_gen);
    printf("  UNSAT con K=%d   : %lld\n", K_start, n_unsat_start);
    printf("  Reducidos a K<=%d : %lld (%.1f%% de UNSAT)\n",
           K_target, n_reduced, n_unsat_start?100.0*n_reduced/n_unsat_start:0);
    if (n_reduced > 0) {
        const char* bud_labels[]={"prop.","<=8","<=25","<=100","<=400","<=2000","NUNCA"};
        printf("\n  CMR de instancias reducidas:\n");
        for (int b=0;b<7;b++)
            if (bud_hist[b])
                printf("    %s: %lld (%.1f%%)\n",
                       bud_labels[b], bud_hist[b], 100.0*bud_hist[b]/n_reduced);
    }
    printf("  Detectados: %lld  Escapadas: %lld\n", n_detected, n_escaped);
    printf("  Tiempo: %.1fs\n", sec);
    printf("================================================================\n");
}

// ─── HARDGEN: búsqueda adversarial sin bt_oracle ─────────────────────────────

void search_hardgen(int N, int K, long long n_att_target, int seed, int n_save_max) {
    printf("================================================================\n");
    printf("  HARDGEN: N=%d  K=%d  intentos=%lld  seed=%d\n",
           N, K, n_att_target, seed);
    printf("  max_combos=%d  depth=%d  n_save=%d\n",
           g_max_combos, g_pipeline_depth, n_save_max);
    printf("  (sin bt_oracle — pipeline+greedy+CMR)\n");
    printf("================================================================\n");
    fflush(stdout);

    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN], sol[MAXN];

    long long n_att=0, n_unsat=0, n_sat=0, n_fail=0, n_saved=0;
    long long bud_hist[7]={};

    struct HardInst {
        int qr[MAXN], qc[MAXN], K;
        int detect_budget, min_dom, avg_dom_x10, ns;
    };
    static const int NTOP=5;
    HardInst top[NTOP]; int ntop=0;

    auto t0 = std::chrono::steady_clock::now();
    long long report_step = std::max(n_att_target/20, (long long)5000);

    while (n_att < n_att_target) {
        n_att++;
        if (!gen_placement_raw(N, K, qr, qc, rng)) continue;

        PResult res = pipeline_solve(qr, qc, K, N, sol);

        if (res == UNSAT_DET) {
            n_unsat++;
            CMRResult cmr = cmr_analyze(qr, qc, K, N);
            int b;
            if      (cmr.detect_budget == 0)    b=0;
            else if (cmr.detect_budget <=   8)  b=1;
            else if (cmr.detect_budget <=  25)  b=2;
            else if (cmr.detect_budget <= 100)  b=3;
            else if (cmr.detect_budget <= 400)  b=4;
            else if (cmr.detect_budget <= 2000) b=5;
            else                                b=6;
            bud_hist[b]++;

            int bsort = (cmr.detect_budget < 0) ? 999999 : cmr.detect_budget;
            int worst_bsort = (ntop==0)?-1:(top[ntop-1].detect_budget<0?999999:top[ntop-1].detect_budget);
            if (ntop < NTOP || bsort > worst_bsort) {
                HardInst h;
                memcpy(h.qr,qr,K*sizeof(int)); memcpy(h.qc,qc,K*sizeof(int));
                h.K=K; h.detect_budget=cmr.detect_budget;
                h.min_dom=cmr.min_dom; h.avg_dom_x10=cmr.avg_dom_x10; h.ns=cmr.ns;
                if (ntop < NTOP) top[ntop++]=h;
                else             top[ntop-1]=h;
                std::sort(top, top+ntop, [](const HardInst& a, const HardInst& b){
                    int ba=(a.detect_budget<0)?999999:a.detect_budget;
                    int bb=(b.detect_budget<0)?999999:b.detect_budget;
                    return ba > bb;
                });
            }
        } else {
            bool complete=true;
            for (int r=0;r<N&&complete;r++) if (sol[r]<0) complete=false;
            if (complete && verify_solution(sol, N)) {
                n_sat++;
            } else {
                n_fail++;
                printf("\n  *** GREEDY_FAIL #%lld  att=%lld ***\n", n_fail, n_att);
                printf("  Reinas:");
                for (int i=0;i<K;i++) printf(" (%d,%d)", qr[i], qc[i]);
                printf("\n");
                if (n_saved < n_save_max) {
                    char fname[256];
                    snprintf(fname,sizeof(fname),"hard_%dx%d_fail%lld.cnf",N,K,n_saved+1);
                    write_completion_cnf(fname, N, qr, qc, K);
                    n_saved++;
                    printf("  [CNF guardado: %s]\n", fname);
                }
                fflush(stdout);
            }
        }

        if (n_att % report_step == 0) {
            float sec=std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
            long long proc=n_unsat+n_sat+n_fail;
            printf("  att=%lld  UNSAT=%lld  SAT=%lld  FAIL=%lld  %.1fs  %.2fus/inst\n",
                   n_att, n_unsat, n_sat, n_fail, sec, 1e6*sec/n_att);
            fflush(stdout);
        }
    }

    float sec=std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
    long long proc=n_unsat+n_sat+n_fail;

    printf("\n================================================================\n");
    printf("  HARDGEN RESULTADO FINAL\n");
    printf("================================================================\n");
    printf("  Intentos      : %lld\n", n_att);
    printf("  UNSAT_DET     : %lld (%.4f%%)\n", n_unsat, 100.0*n_unsat/std::max(proc,1LL));
    printf("  SAT+valido    : %lld (%.4f%%)\n", n_sat,   100.0*n_sat/std::max(proc,1LL));
    printf("  GREEDY_FAIL   : %lld", n_fail);
    if (n_fail==0) printf("  <- pipeline completo en K=%d\n", K);
    else           printf("  <- *** INSTANCIAS DIFICILES: ver hard_*.cnf ***\n");
    if (n_unsat > 0) {
        printf("\n  CMR (minimo budget pivot_enum puro, depth=2):\n");
        const char* labels[]={"  prop.      ","  bud <=  8 ","  bud <= 25 ",
                              "  bud <=100 ","  bud <=400 ","  bud<=2000 ","  NO DETECT "};
        for (int b=0;b<7;b++)
            if (bud_hist[b]) printf("    %s: %lld (%.1f%%)\n",
                labels[b], bud_hist[b], 100.0*bud_hist[b]/n_unsat);
        printf("\n  Top-%d mas duras para pivot_enum puro:\n", ntop);
        for (int i=0;i<ntop;i++) {
            printf("    #%d detect_bud=", i+1);
            if (top[i].detect_budget<0) printf("NUNCA");
            else printf("%d", top[i].detect_budget);
            printf("  min_dom=%d  avg_dom=%.1f  ns=%d\n",
                   top[i].min_dom, top[i].avg_dom_x10/10.0, top[i].ns);
            printf("       queens:");
            for (int j=0;j<top[i].K;j++) printf(" (%d,%d)", top[i].qr[j], top[i].qc[j]);
            printf("\n");
        }
    }
    printf("\n  Tiempo: %.1fs  (%.2f us/inst)\n", sec, 1e6*sec/n_att);
    if (n_fail>0)
        printf("  *** %lld instancias superaron el pipeline: CNF guardados ***\n", n_fail);
    printf("================================================================\n");
}

// ─── BÚSQUEDA PRINCIPAL (usa bt_oracle como oracle de referencia) ─────────────

void search_nodet(int N, int K, int n_target, int seed, int max_show, int k_max) {
    printf("================================================================\n");
    printf("  BUSQUEDA NO_DET: N=%d  K=%d  target=%d  seed=%d  k_max=%d\n",
           N, K, n_target, seed, k_max);
    printf("================================================================\n");
    fflush(stdout);

    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    int n_sat=0, n_unsat=0, n_det=0, n_nodet=0, n_att=0, n_shown=0;
    int k_hist[MAXN+1] = {};

    auto t0 = std::chrono::steady_clock::now();

    while (n_unsat < n_target) {
        n_att++;
        if (!gen_placement_raw(N, K, qr, qc, rng)) continue;

        bool sat = bt_oracle(qr, qc, K, N);
        if (sat) { n_sat++; continue; }

        n_unsat++;
        PResult res = pipeline(qr, qc, K, N);

        if (res == UNSAT_DET) {
            n_det++;
        } else {
            n_nodet++;
            {
                bool used[MAXN]={};
                for (int i=0;i<K;i++) used[qr[i]]=true;
                int singles[MAXN]; int ns=0;
                for (int r=0;r<N;r++) if (!used[r]) singles[ns++]=r;
                bitmask doms[MAXN];
                for (int i=0;i<ns;i++) doms[i]=available_bits(singles[i],N,qr,qc,K);
                ac3_bits(doms, singles, ns, N);
                int sub[MAXN];
                int kf = find_min_incons(doms, singles, ns, k_max, N, sub);
                k_hist[kf]++;
            }
            if (n_shown < max_show) {
                n_shown++;
                printf("\n[NO_DET #%d  intento=%d]\n", n_nodet, n_att);
                analyze_nodet(qr, qc, K, N, k_max);
                fflush(stdout);
            }
        }

        if (n_unsat % 50 == 0 || n_unsat == n_target) {
            float cov = 100.f * n_det / n_unsat;
            auto t1 = std::chrono::steady_clock::now();
            float sec = std::chrono::duration<float>(t1-t0).count();
            printf("  UNSAT=%4d  det=%d(%.0f%%)  nodet=%d  att=%d  %.1fs\n",
                   n_unsat, n_det, cov, n_nodet, n_att, sec);
            fflush(stdout);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    float sec = std::chrono::duration<float>(t1-t0).count();
    float cov = 100.f * n_det / n_unsat;

    printf("\n================================================================\n");
    printf("  RESUMEN FINAL\n");
    printf("================================================================\n");
    printf("  Intentos : %d  SAT: %d  UNSAT: %d\n", n_att, n_sat, n_unsat);
    printf("  Detectadas: %d (%.1f%%)\n", n_det, cov);
    printf("  NO_DET    : %d (%.1f%%)\n", n_nodet, 100.f-cov);
    printf("  Tiempo    : %.1fs\n", sec);
    printf("\n  Distribucion de k-inconsistencia en NO_DET:\n");
    for (int k=0; k<=k_max; k++) {
        if (k_hist[k] > 0) {
            if (k == 0)
                printf("    k>%d (no encontrado): %d casos\n", k_max, k_hist[0]);
            else
                printf("    k=%d: %d casos (%.1f%% de NO_DET)\n",
                       k, k_hist[k], 100.f*k_hist[k]/n_nodet);
        }
    }
    printf("================================================================\n");
}

// ─── TEST GREEDY EN INSTANCIAS UNSAT ─────────────────────────────────────────

void test_greedy_unsat(int N, int K, int n_target, int seed,
                       long long bt_limit, int n_save_cnf) {
    printf("================================================================\n");
    printf("  GREEDY EN UNSAT: N=%d  K=%d  target=%d  bt_limit=%lld  save_cnf=%d\n",
           N, K, n_target, bt_limit, n_save_cnf);
    printf("================================================================\n");
    fflush(stdout);

    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    int n_unsat=0, n_att=0, n_saved=0;
    long long tot_bt=0, max_bt=0, tot_us=0, max_us=0;
    int n_timeout=0;
    int bt_hist[11]={};

    auto t0 = std::chrono::steady_clock::now();

    while (n_unsat < n_target) {
        n_att++;
        if (n_att % 5000 == 0) {
            float sec2 = std::chrono::duration<float>(
                std::chrono::steady_clock::now()-t0).count();
            printf("  [att=%d  unsat=%d  %.1fs]\n", n_att, n_unsat, sec2);
            fflush(stdout);
        }
        if (!gen_placement_raw(N, K, qr, qc, rng)) continue;
        if (bt_oracle(qr, qc, K, N)) continue;

        n_unsat++;
        for (int i=0;i<K;i++) { g_qr[i]=qr[i]; g_qc[i]=qc[i]; }

        long long bt=0;
        auto ti = std::chrono::steady_clock::now();
        bool ok = greedy_rec(K, N, bt, bt_limit);
        auto tf = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(tf-ti).count();

        if (bt >= bt_limit) {
            n_timeout++;
            printf("  inst %3d: TIMEOUT  bt>=%lld  %lldms\n",
                   n_unsat, bt_limit, us/1000);
        } else {
            tot_bt += bt; max_bt = std::max(max_bt, bt);
            tot_us += us; max_us = std::max(max_us, us);
            int bucket = (int)std::min(bt, (long long)10);
            bt_hist[bucket]++;

            if (n_saved < n_save_cnf) {
                char fname[256];
                snprintf(fname, sizeof(fname), "unsat_%dx%d_%04d.cnf", N, K, n_saved+1);
                write_completion_cnf(fname, N, qr, qc, K);
                n_saved++;
                printf("  [CNF guardado: %s  bt=%lld  %.2fms]\n",
                       fname, bt, us/1000.0);
            } else if (n_unsat <= 20 || bt > 100) {
                printf("  inst %3d: UNSAT  bt=%lld  %lldus  (%.2fms)\n",
                       n_unsat, bt, us, us/1000.0);
            }
        }
        fflush(stdout);
    }

    auto t1 = std::chrono::steady_clock::now();
    float sec = std::chrono::duration<float>(t1-t0).count();
    int n_ok = n_unsat - n_timeout;

    printf("\n================================================================\n");
    printf("  RESUMEN GREEDY EN UNSAT\n");
    printf("================================================================\n");
    printf("  Instancias UNSAT: %d  Timeouts: %d\n", n_unsat, n_timeout);
    printf("  Backtracks: avg=%.1f  max=%lld\n",
           n_ok ? (float)tot_bt/n_ok : 0, max_bt);
    printf("  Tiempo/inst: avg=%.2fms  max=%.2fms\n",
           n_ok ? tot_us/1000.0/n_ok : 0, max_us/1000.0);
    printf("  Tiempo total: %.1fs\n", sec);
    printf("\n  Distribucion de backtracks:\n");
    for (int b=0; b<=10; b++) {
        if (bt_hist[b]>0) {
            if (b<10) printf("    bt=%d: %d inst\n", b, bt_hist[b]);
            else      printf("    bt>=10: %d inst\n", bt_hist[b]);
        }
    }
    printf("================================================================\n");
}

// ─── MODO LARGE: greedy como oracle + guardar CNF ────────────────────────────

void test_greedy_large(int N, int K, int n_unsat_target, int seed,
                       long long bt_limit, int n_save_cnf, long long bt_min_save) {
    printf("================================================================\n");
    printf("  GREEDY LARGE: N=%d  K=%d  target_unsat=%d  bt_limit=%lld\n",
           N, K, n_unsat_target, bt_limit);
    printf("  (greedy como oracle — sin bt_oracle)\n");
    printf("================================================================\n");
    fflush(stdout);

    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    int n_sat=0, n_unsat=0, n_att=0, n_skip=0, n_saved=0;
    long long tot_bt=0, max_bt=0, tot_us=0, max_us=0;
    int bt_hist[12]={};

    auto t0 = std::chrono::steady_clock::now();

    while (n_unsat < n_unsat_target) {
        n_att++;
        if (!gen_placement_raw(N, K, qr, qc, rng)) continue;

        for (int i=0;i<K;i++) { g_qr[i]=qr[i]; g_qc[i]=qc[i]; }
        long long bt=0;
        auto ti = std::chrono::steady_clock::now();
        bool ok = greedy_rec(K, N, bt, bt_limit);
        auto tf = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(tf-ti).count();

        if (ok) { n_sat++; continue; }
        if (bt >= bt_limit) { n_skip++; continue; }

        n_unsat++;
        tot_bt += bt; max_bt = std::max(max_bt, bt);
        tot_us += us; max_us = std::max(max_us, us);
        int bucket = (int)std::min(bt, (long long)11);
        bt_hist[bucket]++;

        if (n_saved < n_save_cnf && bt >= bt_min_save) {
            char fname[256];
            snprintf(fname, sizeof(fname), "unsat_%dx%d_%04d.cnf", N, K, n_saved+1);
            write_completion_cnf(fname, N, qr, qc, K);
            n_saved++;
            printf("  [CNF guardado: %s  bt=%lld  %.2fms]\n", fname, bt, us/1000.0);
        }

        if (n_unsat <= 30 || bt > 50) {
            printf("  inst %4d: UNSAT  bt=%lld  %.2fms  (att=%d)\n",
                   n_unsat, bt, us/1000.0, n_att);
        }
        if (n_unsat % 20 == 0) {
            float sec2 = std::chrono::duration<float>(
                std::chrono::steady_clock::now()-t0).count();
            printf("  -- SAT=%d UNSAT=%d skip=%d att=%d  %.1fs --\n",
                   n_sat, n_unsat, n_skip, n_att, sec2);
            fflush(stdout);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    float sec = std::chrono::duration<float>(t1-t0).count();

    printf("\n================================================================\n");
    printf("  RESUMEN\n");
    printf("================================================================\n");
    printf("  Intentos: %d  SAT: %d  UNSAT: %d  Skip: %d\n",n_att,n_sat,n_unsat,n_skip);
    printf("  BT: avg=%.1f  max=%lld\n", n_unsat?(float)tot_bt/n_unsat:0, max_bt);
    printf("  Tiempo/inst: avg=%.2fms  max=%.2fms\n",
           n_unsat?tot_us/1000.0/n_unsat:0, max_us/1000.0);
    printf("  Tiempo total: %.1fs\n", sec);
    printf("  CNF guardados: %d (unsat_%dx%d_NNNN.cnf)\n", n_saved, N, K);
    printf("\n  BT distribution:\n");
    for (int b=0;b<=11;b++) if(bt_hist[b]>0) {
        if (b<11) printf("    bt=%d: %d\n", b, bt_hist[b]);
        else      printf("    bt>=11: %d\n", bt_hist[b]);
    }
    printf("================================================================\n");
}

// ─── MODO COLLECT: CSV nocturno para análisis IA ─────────────────────────────

void collect_nodet(int N, int K, int seed, long long bt_limit,
                   const char* outfile, long long max_collect) {
    printf("================================================================\n");
    printf("  COLLECT MODE: N=%d  K=%d  seed=%d  output=%s\n", N, K, seed, outfile);
    printf("  Recolectando UNSAT NO_DET (bt>0)... Ctrl+C para detener.\n");
    printf("================================================================\n");
    fflush(stdout);

    FILE* fout = fopen(outfile, "w");
    if (!fout) { printf("ERROR abriendo %s\n", outfile); return; }

    fprintf(fout, "bt,us");
    for (int i=0; i<N-K; i++) fprintf(fout, ",dom%d", i);
    for (int i=0; i<K; i++) fprintf(fout, ",qr%d,qc%d", i, i);
    fprintf(fout, "\n");
    fflush(fout);

    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    long long n_collect=0, n_att=0, n_sat=0, n_easy=0, n_skip=0;
    long long tot_bt=0, max_bt=0;

    auto t0 = std::chrono::steady_clock::now();

    while (max_collect <= 0 || n_collect < max_collect) {
        n_att++;
        if (!gen_placement_raw(N, K, qr, qc, rng)) continue;

        for (int i=0;i<K;i++) { g_qr[i]=qr[i]; g_qc[i]=qc[i]; }
        long long bt=0;
        auto ti = std::chrono::steady_clock::now();
        bool ok = greedy_rec(K, N, bt, bt_limit);
        auto tf = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(tf-ti).count();

        if (ok) { n_sat++; continue; }
        if (bt >= bt_limit) { n_skip++; continue; }
        if (bt == 0) { n_easy++; continue; }

        n_collect++;
        tot_bt += bt; max_bt = std::max(max_bt, bt);

        bool used[MAXN]={};
        for (int i=0;i<K;i++) used[qr[i]]=true;
        int singles[MAXN]; int ns=0;
        for (int r=0;r<N;r++) if (!used[r]) singles[ns++]=r;

        bitmask doms[MAXN];
        for (int i=0;i<ns;i++) doms[i] = available_bits(singles[i], N, qr, qc, K);
        ac3_bits(doms, singles, ns, N);

        fprintf(fout, "%lld,%lld", bt, us);
        for (int i=0;i<ns;i++) fprintf(fout, ",%llu", (unsigned long long)doms[i]);
        for (int i=0;i<K;i++) fprintf(fout, ",%d,%d", qr[i], qc[i]);
        fprintf(fout, "\n");

        if (n_collect % 500 == 0) {
            fflush(fout);
            auto now = std::chrono::steady_clock::now();
            float sec = std::chrono::duration<float>(now-t0).count();
            float rate = n_collect / sec;
            printf("  [collect=%lld  att=%lld  easy=%lld  sat=%lld  skip=%lld"
                   "  avg_bt=%.1f  max_bt=%lld  rate=%.1f/s  %.0fs]\n",
                   n_collect, n_att, n_easy, n_sat, n_skip,
                   (float)tot_bt/n_collect, max_bt, rate, sec);
            fflush(stdout);
        }
    }

    fclose(fout);
    float sec = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
    printf("\n  Terminado: %lld instancias NO_DET en %.1fs -> %s\n",
           n_collect, sec, outfile);
}

// ─── TEST MEJORAS ─────────────────────────────────────────────────────────────

void test_mejoras(int N, int K, const char* fname, long long n_limit) {
    FILE* fp = fopen(fname, "r");
    if (!fp) { printf("Error abriendo %s\n", fname); return; }
    int n_libres = N - K;

    char buf[131072];
    fgets(buf, sizeof(buf), fp);

    long long n_read = 0;
    long long n_det_hall = 0, n_det_triple = 0, n_det_both = 0;
    long long n_nodet_remain = 0;
    long long bt_orig_total = 0, bt_ng_total = 0;
    long long bt_orig_max = 0, bt_ng_max = 0;
    int qr[MAXN], qc[MAXN];

    printf("================================================================\n");
    printf("  TEST MEJORAS: N=%d K=%d  archivo=%s\n", N, K, fname);
    if (n_limit > 0) printf("  Limite: %lld instancias\n", n_limit);
    printf("================================================================\n");

    while (!feof(fp) && (n_limit == 0 || n_read < n_limit)) {
        long long bt_orig, us;
        if (fscanf(fp, "%lld,%lld", &bt_orig, &us) != 2) break;

        bitmask doms_csv[MAXN];
        bool ok = true;
        for (int i = 0; i < n_libres && ok; i++) {
            unsigned long long d;
            if (fscanf(fp, ",%llu", &d) != 1) { ok = false; break; }
            doms_csv[i] = (bitmask)d;
        }
        for (int i = 0; i < K && ok; i++) {
            if (fscanf(fp, ",%d,%d", &qr[i], &qc[i]) != 2) { ok = false; break; }
        }
        fgets(buf, sizeof(buf), fp);
        if (!ok) break;

        n_read++;
        bt_orig_total += bt_orig;
        if (bt_orig > bt_orig_max) bt_orig_max = bt_orig;

        g_hall_enabled = true; g_triple_enabled = false;
        bool by_hall   = (pipeline(qr, qc, K, N) == UNSAT_DET);

        g_hall_enabled = false; g_triple_enabled = true;
        bool by_triple = (pipeline(qr, qc, K, N) == UNSAT_DET);

        g_hall_enabled = true; g_triple_enabled = true;

        if (by_hall)   n_det_hall++;
        if (by_triple) n_det_triple++;
        if (by_hall || by_triple) { n_det_both++; continue; }

        n_nodet_remain++;
        s_ng_enabled = true;
        s_nogoods.clear();
        memcpy(g_qr, qr, K * sizeof(int));
        memcpy(g_qc, qc, K * sizeof(int));
        long long bt_ng = 0;
        long long bt_lim = std::max(bt_orig * 4, 1000LL);
        greedy_rec(K, N, bt_ng, bt_lim);
        s_ng_enabled = false;
        bt_ng_total += bt_ng;
        if (bt_ng > bt_ng_max) bt_ng_max = bt_ng;

        if (n_nodet_remain <= 5)
            printf("  [muestra NO_DET] bt_orig=%lld  bt_ng=%lld  ng_store=%d\n",
                   bt_orig, bt_ng, (int)s_nogoods.size());
    }

    fclose(fp);

    long long n_det_total = n_det_both;
    long long n_nodet_total = n_read - n_det_total;
    printf("\n================================================================\n");
    printf("  RESULTADOS\n");
    printf("================================================================\n");
    printf("  Instancias leidas       : %lld\n", n_read);
    printf("  Solo por Hall           : %lld (%.1f%%)\n",
           n_det_hall,   100.f * n_det_hall   / n_read);
    printf("  Solo por Triple         : %lld (%.1f%%)\n",
           n_det_triple, 100.f * n_det_triple / n_read);
    printf("  Total detectadas        : %lld (%.1f%%)\n",
           n_det_total,  100.f * n_det_total  / n_read);
    printf("  Siguen NO_DET           : %lld (%.1f%%)\n",
           n_nodet_total, 100.f * n_nodet_total / n_read);
    if (n_nodet_remain > 0) {
        printf("\n  No-goods en NO_DET restantes:\n");
        printf("  bt original  avg=%.1f  max=%lld\n",
               (float)bt_orig_total / n_read, bt_orig_max);
        printf("  bt con ng    avg=%.1f  max=%lld  (solo NO_DET: %lld inst)\n",
               (float)bt_ng_total / n_nodet_remain, bt_ng_max, n_nodet_remain);
        if (bt_orig_total > 0)
            printf("  Reduccion bt (NO_DET): %.2fx\n",
                   (float)bt_orig_total / std::max(bt_ng_total, 1LL));
    }
    printf("================================================================\n");
}

// ─── PATRON: analiza features geometricos de instancias UNSAT por budget ─────

struct PatronFeats {
    int budget;
    int min_row_gap, max_row_gap;
    int min_col_diff, max_col_diff, col_spread;
    float row_uniformity; // 0=perfecto N/K, mayor=mas irregular
    int qr[MAXN], qc[MAXN], K;
};

static void compute_feats(const int* qr, const int* qc, int K, int N, int budget, PatronFeats& f) {
    f.budget = budget; f.K = K;
    memcpy(f.qr, qr, K*sizeof(int)); memcpy(f.qc, qc, K*sizeof(int));
    // rows ya ordenados por construccion (next_row creciente)
    f.min_row_gap = N; f.max_row_gap = 0;
    float ideal = (float)N / K;
    float usum = 0;
    for (int i=0;i<K-1;i++) {
        int g = qr[i+1]-qr[i];
        if (g < f.min_row_gap) f.min_row_gap = g;
        if (g > f.max_row_gap) f.max_row_gap = g;
        usum += (g - ideal)*(g - ideal);
    }
    f.row_uniformity = (K>1) ? usum/(K-1) : 0;
    int cmin=N, cmax=0;
    f.min_col_diff = N; f.max_col_diff = 0;
    for (int i=0;i<K;i++) {
        if (qc[i]<cmin) cmin=qc[i]; if (qc[i]>cmax) cmax=qc[i];
        for (int j=i+1;j<K;j++) {
            int d = abs(qc[i]-qc[j]);
            if (d < f.min_col_diff) f.min_col_diff = d;
            if (d > f.max_col_diff) f.max_col_diff = d;
        }
    }
    f.col_spread = cmax - cmin;
}

void patron_kmin(int N, int K, const char* outfile) {
    printf("================================================================\n");
    printf("  PATRON K_MIN: N=%d  K=%d -> %s\n", N, K, outfile);
    printf("  max_combos=%d  depth=%d\n", g_max_combos, g_pipeline_depth);
    printf("================================================================\n");
    fflush(stdout);

    std::vector<PatronFeats> all_unsat;
    bool uc[MAXN]={}, uu[2*MAXN]={}, ud_arr[2*MAXN]={};
    int qr[MAXN], qc[MAXN];

    // reutilizamos cen_rec pero necesitamos capturar con features
    std::function<void(int,int,bool*,bool*,bool*)> rec = [&](int placed, int next_row, bool* uc2, bool* uu2, bool* ud2) {
        if (placed == K) {
            PResult res = pipeline(qr, qc, K, N);
            if (res != UNSAT_DET) return;
            CMRResult cmr = cmr_analyze(qr, qc, K, N);
            PatronFeats f;
            compute_feats(qr, qc, K, N, cmr.detect_budget, f);
            all_unsat.push_back(f);
            return;
        }
        int rows_left = K - placed;
        for (int r = next_row; r <= N - rows_left; r++) {
            for (int c = 0; c < N; c++) {
                if (uc2[c]) continue;
                int ui=r-c+N, di=r+c;
                if (uu2[ui]||ud2[di]) continue;
                qr[placed]=r; qc[placed]=c;
                uc2[c]=true; uu2[ui]=true; ud2[di]=true;
                rec(placed+1, r+1, uc2, uu2, ud2);
                uc2[c]=false; uu2[ui]=false; ud2[di]=false;
            }
        }
    };

    auto t0 = std::chrono::steady_clock::now();
    rec(0, 0, uc, uu, ud_arr);
    float sec = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();

    printf("  Total UNSAT: %zu  t=%.2fs\n", all_unsat.size(), sec);

    // Ordenar por budget descendente
    std::sort(all_unsat.begin(), all_unsat.end(), [](const PatronFeats& a, const PatronFeats& b){
        int ba=(a.budget<0)?999999:a.budget, bb=(b.budget<0)?999999:b.budget;
        return ba > bb;
    });

    // Agrupar por nivel de budget y calcular promedios
    auto bname = [](int b) -> const char* {
        if(b==0) return "prop.";
        if(b<=8) return "<=8";
        if(b<=25) return "<=25";
        if(b<=100) return "<=100";
        if(b<=400) return "<=400";
        if(b<=2000) return "<=2000";
        return "NEVER";
    };
    auto blast = -2;
    int gcnt=0; float g_rg=0,g_cd=0,g_cs=0,g_ru=0;
    printf("\n  Patron por nivel de budget:\n");
    printf("  %-8s %6s %8s %8s %8s %8s\n","budget","count","min_rgap","min_cdiff","col_sprd","row_unif");
    auto flush_group = [&]() {
        if (gcnt==0) return;
        printf("  %-8s %6d %8.1f %8.1f %8.1f %8.2f\n",
               bname(blast), gcnt, g_rg/gcnt, g_cd/gcnt, g_cs/gcnt, g_ru/gcnt);
        gcnt=0; g_rg=g_cd=g_cs=g_ru=0;
    };

    for (auto& f : all_unsat) {
        int bl = (f.budget<=0)?0:(f.budget<=8)?1:(f.budget<=25)?2:
                 (f.budget<=100)?3:(f.budget<=400)?4:(f.budget<=2000)?5:6;
        if (bl != blast) { flush_group(); blast=bl; }
        g_rg += f.min_row_gap; g_cd += f.min_col_diff;
        g_cs += f.col_spread; g_ru += f.row_uniformity;
        gcnt++;
    }
    flush_group();

    // Guardar archivo con todas las instancias
    FILE* fp = fopen(outfile, "w");
    if (fp) {
        fprintf(fp, "budget,min_row_gap,max_row_gap,min_col_diff,col_spread,row_uniformity,queens\n");
        for (auto& f : all_unsat) {
            fprintf(fp, "%d,%d,%d,%d,%d,%.2f,", f.budget,
                    f.min_row_gap, f.max_row_gap, f.min_col_diff, f.col_spread, f.row_uniformity);
            for (int i=0;i<f.K;i++) fprintf(fp,"(%d,%d)%s",f.qr[i],f.qc[i],i<f.K-1?"|":"");
            fprintf(fp,"\n");
        }
        fclose(fp);
        printf("\n  Guardado: %s (%zu instancias)\n", outfile, all_unsat.size());
    }

    // Mostrar top-20 mas duras
    printf("\n  Top instancias mas duras:\n");
    int show = std::min((int)all_unsat.size(), 20);
    for (int i=0;i<show;i++) {
        auto& f = all_unsat[i];
        printf("  bud=%-4d rg_min=%d cd_min=%d cs=%d  queens:",
               f.budget, f.min_row_gap, f.min_col_diff, f.col_spread);
        for (int j=0;j<f.K;j++) printf(" (%d,%d)",f.qr[j],f.qc[j]);
        printf("\n");
    }
    printf("================================================================\n");
}

// ─── PATRON_SAMPLED: patron con limite de instancias (para N grandes) ─────────

static long long s_ps_max   = 0;   // limite total de colocaciones
static long long s_ps_total = 0, s_ps_unsat = 0;
static std::vector<PatronFeats> s_ps_found;

static void ps_rec(int placed, int K, int N, int* qr, int* qc,
                   int next_row, bool* uc, bool* uu, bool* ud) {
    if (s_ps_max > 0 && s_ps_total >= s_ps_max) return;
    if (placed == K) {
        s_ps_total++;
        PResult res = pipeline(qr, qc, K, N);
        if (res != UNSAT_DET) return;
        s_ps_unsat++;
        CMRResult cmr = cmr_analyze(qr, qc, K, N);
        PatronFeats f; compute_feats(qr, qc, K, N, cmr.detect_budget, f);
        s_ps_found.push_back(f);
        return;
    }
    int rows_left = K - placed;
    for (int r = next_row; r <= N - rows_left; r++) {
        if (s_ps_max > 0 && s_ps_total >= s_ps_max) return;
        for (int c = 0; c < N; c++) {
            if (s_ps_max > 0 && s_ps_total >= s_ps_max) return;
            if (uc[c]) continue;
            int ui=r-c+N, di=r+c;
            if (uu[ui]||ud[di]) continue;
            qr[placed]=r; qc[placed]=c;
            uc[c]=true; uu[ui]=true; ud[di]=true;
            ps_rec(placed+1, K, N, qr, qc, r+1, uc, uu, ud);
            uc[c]=false; uu[ui]=false; ud[di]=false;
        }
    }
}

void patron_sampled(int N, int K, long long max_total) {
    printf("--- patron_sampled N=%d K=%d max=%lld ---\n", N, K, max_total);
    fflush(stdout);
    bool uc[MAXN]={}, uu[2*MAXN]={}, ud[2*MAXN]={};
    int qr[MAXN], qc[MAXN];
    s_ps_max=max_total; s_ps_total=0; s_ps_unsat=0; s_ps_found.clear();
    auto t0 = std::chrono::steady_clock::now();
    ps_rec(0, K, N, qr, qc, 0, uc, uu, ud);
    float sec = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
    bool full = (s_ps_max==0 || s_ps_total < s_ps_max);
    printf("  total=%lld unsat=%lld %s t=%.2fs\n",
           s_ps_total, s_ps_unsat, full?"(completo)":"(muestra)", sec);
    if (s_ps_found.empty()) { printf("  -> 0 UNSAT en muestra\n\n"); return; }
    // sort by budget desc
    std::sort(s_ps_found.begin(), s_ps_found.end(), [](const PatronFeats& a, const PatronFeats& b){
        return ((a.budget<0)?999999:a.budget) > ((b.budget<0)?999999:b.budget);
    });
    // stats por nivel
    auto bname=[](int b)->const char*{
        return b==0?"prop.":b<=8?"<=8":b<=25?"<=25":b<=100?"<=100":b<=400?"<=400":b<=2000?"<=2000":"NEVER";
    };
    int blast=-2,gcnt=0; float g_rg=0,g_cs=0;
    auto flush_g=[&](){
        if(!gcnt) return;
        printf("  %-6s cnt=%4d  avg_min_rgap=%.1f  avg_col_spread=%.1f\n",
               bname(blast),gcnt,g_rg/gcnt,g_cs/gcnt);
        gcnt=0; g_rg=g_cs=0;
    };
    for (auto& f: s_ps_found) {
        int bl=(f.budget<=0)?0:(f.budget<=8)?1:(f.budget<=25)?2:(f.budget<=100)?3:
               (f.budget<=400)?4:(f.budget<=2000)?5:6;
        if(bl!=blast){flush_g();blast=bl;}
        g_rg+=f.min_row_gap; g_cs+=f.col_spread; gcnt++;
    }
    flush_g();
    int show=std::min((int)s_ps_found.size(),8);
    printf("  Top-%d: ",show);
    for(int i=0;i<show;i++){
        auto& f=s_ps_found[i];
        printf("bud=%d[",f.budget);
        for(int j=0;j<f.K;j++) printf("(%d,%d)%s",f.qr[j],f.qc[j],j<f.K-1?"|":"");
        printf("] ");
    }
    printf("\n\n");
    fflush(stdout);
}

// ─── KMIN_SWEEP: encuentra K_min exhaustivo para N_lo..N_hi ──────────────────

static bool    s_es_stop  = false;
static long long s_es_total = 0, s_es_unsat = 0;

static void es_rec(int placed, int K, int N, int* qr, int* qc,
                   int next_row, bool* uc, bool* uu, bool* ud) {
    if (s_es_stop) return;
    if (placed == K) {
        s_es_total++;
        if (pipeline(qr, qc, K, N) == UNSAT_DET) { s_es_unsat++; s_es_stop = true; }
        return;
    }
    int rows_left = K - placed;
    for (int r = next_row; r <= N - rows_left && !s_es_stop; r++) {
        for (int c = 0; c < N && !s_es_stop; c++) {
            if (uc[c]) continue;
            int ui=r-c+N, di=r+c;
            if (uu[ui]||ud[di]) continue;
            qr[placed]=r; qc[placed]=c;
            uc[c]=true; uu[ui]=true; ud[di]=true;
            es_rec(placed+1, K, N, qr, qc, r+1, uc, uu, ud);
            uc[c]=false; uu[ui]=false; ud[di]=false;
        }
    }
}

void kmin_sweep(int N_lo, int N_hi) {
    printf("================================================================\n");
    printf("  KMIN SWEEP: N=%d..%d\n", N_lo, N_hi);
    printf("================================================================\n");
    printf("  N  | K_min | N/4    | floor | ceil | match\n");
    printf("  ---|-------|--------|-------|------|------\n");
    fflush(stdout);

    for (int N = N_lo; N <= N_hi; N++) {
        bool uc[MAXN]={}, uu[2*MAXN]={}, ud[2*MAXN]={};
        int qr[MAXN], qc[MAXN];
        int kmin = -1; bool timeout = false;

        for (int K = 1; K <= N && kmin < 0 && !timeout; K++) {
            s_es_total=s_es_unsat=0; s_es_stop=false;
            auto t0 = std::chrono::steady_clock::now();
            es_rec(0, K, N, qr, qc, 0, uc, uu, ud);
            float sec = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
            if (s_es_unsat > 0) kmin = K;
            else if (sec > 10.0f) timeout = true;
        }

        int fl = N/4, ce = (N+3)/4;
        const char* match = (kmin==fl)?"= floor":(kmin==ce)?"= ceil":"OTHER";
        if (kmin < 0) match = timeout ? ">= K (timeout)" : "?";
        printf("  %2d | %5s | %6.2f | %5d | %4d | %s\n",
               N, kmin<0?">=K":std::to_string(kmin).c_str(),
               N/4.0, fl, ce, match);
        fflush(stdout);
    }
    printf("================================================================\n");
}

// ─── MCV_TEST: compara budget SIN vs CON MCV en mismas instancias UNSAT ──────

void mcv_test(int N, int K, long long n_target, int seed) {
    printf("================================================================\n");
    printf("  MCV_TEST: N=%d  K=%d  target_UNSAT=%lld  seed=%d\n", N, K, n_target, seed);
    printf("  max_combos=%d  depth=%d\n", g_max_combos, g_pipeline_depth);
    printf("================================================================\n");
    fflush(stdout);

    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    long long n_att=0, n_unsat=0;
    long long bud_base[7]={}, bud_mcv[7]={};
    long long sum_base=0, sum_mcv=0, max_base=0, max_mcv=0;
    int n_better=0, n_worse=0, n_equal=0;

    auto t0 = std::chrono::steady_clock::now();

    while (n_unsat < n_target) {
        n_att++;
        if (!gen_placement_raw(N, K, qr, qc, rng)) continue;

        // Clasificar sin MCV
        g_mcv_enabled = false;
        PResult res = pipeline(qr, qc, K, N);
        if (res != UNSAT_DET) continue;

        n_unsat++;
        CMRResult r0 = cmr_analyze(qr, qc, K, N);
        int b0 = (r0.detect_budget==0)?0:(r0.detect_budget<=8)?1:
                 (r0.detect_budget<=25)?2:(r0.detect_budget<=100)?3:
                 (r0.detect_budget<=400)?4:(r0.detect_budget<=2000)?5:6;
        bud_base[b0]++;
        int bv0 = (r0.detect_budget<0)?g_max_combos:r0.detect_budget;
        sum_base += bv0; if(bv0>max_base) max_base=bv0;

        // Mismo instancia CON MCV
        g_mcv_enabled = true;
        CMRResult r1 = cmr_analyze(qr, qc, K, N);
        g_mcv_enabled = false;
        int b1 = (r1.detect_budget==0)?0:(r1.detect_budget<=8)?1:
                 (r1.detect_budget<=25)?2:(r1.detect_budget<=100)?3:
                 (r1.detect_budget<=400)?4:(r1.detect_budget<=2000)?5:6;
        bud_mcv[b1]++;
        int bv1 = (r1.detect_budget<0)?g_max_combos:r1.detect_budget;
        sum_mcv += bv1; if(bv1>max_mcv) max_mcv=bv1;

        if      (bv1 < bv0) n_better++;
        else if (bv1 > bv0) n_worse++;
        else                n_equal++;

        // Mostrar instancias donde MCV ayuda mucho
        if (bv0 > 8 && bv1 < bv0/2) {
            printf("  [MEJORA x%.1f] bud_base=%d -> bud_mcv=%d  queens:",
                   (float)bv0/std::max(bv1,1), bv0, bv1);
            for (int i=0;i<K;i++) printf(" (%d,%d)",qr[i],qc[i]);
            printf("\n"); fflush(stdout);
        }

        if (n_unsat % 10 == 0) {
            float sec = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
            printf("  UNSAT=%lld  att=%lld  mejor=%d  igual=%d  peor=%d  %.1fs\n",
                   n_unsat, n_att, n_better, n_equal, n_worse, sec);
            fflush(stdout);
        }
    }

    float sec = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();
    const char* lbl[]={"prop.","<=8","<=25","<=100","<=400","<=2000","NEVER"};

    printf("\n================================================================\n");
    printf("  RESULTADO MCV_TEST\n");
    printf("================================================================\n");
    printf("  Instancias UNSAT: %lld  (att=%lld)\n", n_unsat, n_att);
    printf("  Budget avg: BASE=%.1f  MCV=%.1f  (ratio=%.2fx)\n",
           (float)sum_base/n_unsat, (float)sum_mcv/n_unsat,
           (float)sum_base/std::max(sum_mcv,1LL));
    printf("  Budget max: BASE=%lld  MCV=%lld\n", max_base, max_mcv);
    printf("  MCV mejora: %d (%.1f%%)  igual: %d  peor: %d\n",
           n_better, 100.f*n_better/n_unsat, n_equal, n_worse);
    printf("\n  Distribucion BASE:\n");
    for (int b=0;b<7;b++) if(bud_base[b]) printf("    %s: %lld\n",lbl[b],bud_base[b]);
    printf("  Distribucion MCV:\n");
    for (int b=0;b<7;b++) if(bud_mcv[b]) printf("    %s: %lld\n",lbl[b],bud_mcv[b]);
    printf("  Tiempo: %.1fs\n", sec);
    printf("================================================================\n");
}

// ─── CENSUS EXHAUSTIVO ────────────────────────────────────────────────────────
// Genera TODAS las colocaciones no-atacantes de K reinas en N×N,
// clasifica cada una con el pipeline, reporta distribución por budget.

static long long s_cen_total, s_cen_unsat, s_cen_sat;
static long long s_cen_bud[7]; // 0=prop 1=<=8 2=<=25 3=<=100 4=<=400 5=<=2000 6=NEVER

struct HardInst2 { int qr[MAXN], qc[MAXN], K, budget; };
static HardInst2 s_cen_top[5]; static int s_cen_ntop;

static void cen_push(const int* qr, const int* qc, int K, int budget) {
    int bsort = (budget < 0) ? 999999 : budget;
    if (s_cen_ntop < 5 || bsort > ((s_cen_top[s_cen_ntop-1].budget<0)?999999:s_cen_top[s_cen_ntop-1].budget)) {
        HardInst2 h; memcpy(h.qr,qr,K*sizeof(int)); memcpy(h.qc,qc,K*sizeof(int));
        h.K=K; h.budget=budget;
        if (s_cen_ntop < 5) s_cen_top[s_cen_ntop++]=h;
        else s_cen_top[4]=h;
        std::sort(s_cen_top, s_cen_top+s_cen_ntop, [](const HardInst2& a, const HardInst2& b){
            return ((a.budget<0)?999999:a.budget) > ((b.budget<0)?999999:b.budget);
        });
    }
}

static void cen_rec(int placed, int K, int N, int* qr, int* qc,
                    int next_row, bool* uc, bool* uu, bool* ud) {
    if (placed == K) {
        s_cen_total++;
        PResult res = pipeline(qr, qc, K, N);
        if (res != UNSAT_DET) { s_cen_sat++; return; }
        s_cen_unsat++;
        CMRResult cmr = cmr_analyze(qr, qc, K, N);
        int b = (cmr.detect_budget==0)?0:(cmr.detect_budget<=8)?1:
                (cmr.detect_budget<=25)?2:(cmr.detect_budget<=100)?3:
                (cmr.detect_budget<=400)?4:(cmr.detect_budget<=2000)?5:6;
        s_cen_bud[b]++;
        cen_push(qr, qc, K, cmr.detect_budget);
        return;
    }
    int rows_left = K - placed;
    for (int r = next_row; r <= N - rows_left; r++) {
        for (int c = 0; c < N; c++) {
            if (uc[c]) continue;
            int ui = r - c + N, di = r + c;
            if (uu[ui] || ud[di]) continue;
            qr[placed]=r; qc[placed]=c;
            uc[c]=true; uu[ui]=true; ud[di]=true;
            cen_rec(placed+1, K, N, qr, qc, r+1, uc, uu, ud);
            uc[c]=false; uu[ui]=false; ud[di]=false;
        }
    }
}

void census_exhaustive(int N, int K_lo, int K_hi) {
    printf("================================================================\n");
    printf("  CENSUS EXHAUSTIVO: N=%d  K=%d..%d\n", N, K_lo, K_hi);
    printf("  max_combos=%d  depth=%d\n", g_max_combos, g_pipeline_depth);
    printf("================================================================\n");
    fflush(stdout);

    bool uc[MAXN]={}, uu[2*MAXN]={}, ud[2*MAXN]={};
    int qr[MAXN], qc[MAXN];

    for (int K = K_lo; K <= K_hi; K++) {
        s_cen_total=s_cen_unsat=s_cen_sat=0;
        memset(s_cen_bud,0,sizeof(s_cen_bud));
        s_cen_ntop=0;

        auto t0 = std::chrono::steady_clock::now();
        cen_rec(0, K, N, qr, qc, 0, uc, uu, ud);
        float sec = std::chrono::duration<float>(std::chrono::steady_clock::now()-t0).count();

        printf("\n--- K=%d ---\n", K);
        printf("  Total: %lld  UNSAT: %lld (%.2f%%)  SAT: %lld  t=%.2fs\n",
               s_cen_total, s_cen_unsat,
               100.0*s_cen_unsat/std::max(s_cen_total,1LL),
               s_cen_sat, sec);
        if (s_cen_unsat > 0) {
            const char* lbl[]={"prop.","<=8","<=25","<=100","<=400","<=2000","NEVER"};
            printf("  Budget dist: ");
            for (int b=0;b<7;b++) if(s_cen_bud[b]) printf("%s:%lld ", lbl[b], s_cen_bud[b]);
            printf("\n  Top-%d mas duras:\n", s_cen_ntop);
            for (int i=0;i<s_cen_ntop;i++) {
                printf("    #%d bud=", i+1);
                if (s_cen_top[i].budget<0) printf("NEVER"); else printf("%d",s_cen_top[i].budget);
                printf(" queens:");
                for (int j=0;j<K;j++) printf(" (%d,%d)",s_cen_top[i].qr[j],s_cen_top[i].qc[j]);
                printf("\n");
            }
        }
        fflush(stdout);
    }
    printf("\n================================================================\n");
    printf("  CENSO COMPLETADO\n");
    printf("================================================================\n");
}

// ─── SWEEP_PIPELINE ──────────────────────────────────────────────────────────
// Para cada N en rango: halla instancias K_min UNSAT (K=N/4), corre CMR con
// timing, computa patron geometrico (centroide vs centro tablero).
// Todo en C++, sin overhead de subprocess.

struct SInst { int qr[MAXN], qc[MAXN]; };

// Carga instancias de patron_N{N}_K{K}.csv (formato: lineas con pares (r,c))
static int sp_load_csv(const char* fname, int K, std::vector<SInst>& out, int cap) {
    FILE* f = fopen(fname, "r");
    if (!f) return 0;
    char line[8192];
    bool first = true;
    int added = 0;
    while (fgets(line, sizeof(line), f) && added < cap) {
        if (first) { first = false; continue; }
        SInst inst;
        int ki = 0;
        const char* p = line;
        while (ki < K) {
            while (*p && *p != '(') p++;
            if (!*p) break;
            int r, c;
            if (sscanf(p, "(%d,%d)", &r, &c) == 2) {
                inst.qr[ki] = r; inst.qc[ki] = c; ki++;
            }
            while (*p && *p != ')') p++;
            if (*p) p++;
        }
        if (ki == K) { out.push_back(inst); added++; }
    }
    fclose(f);
    return added;
}

// K_min empírico para N dado. Tabla verificada N<=19, fórmula para N>=20.
static int get_kmin(int N) {
    static const int tbl[] = {0,0,0,0,1,2,1,2,2,2,3,3,3,3,4,4,4,4,5,5};
    if (N < (int)(sizeof(tbl)/sizeof(tbl[0])))
        return std::max(1, tbl[N]);
    return (N % 4 == 1) ? N/4 : (N+3)/4;
}

// K_start preferido para shrink: 2K reducido hasta K+1 si necesario.
// Retorna -1 si imposible (K+1 >= N).
static int kstart_for(int K, int N) {
    int ks = 2 * K;
    if (ks >= N) ks = K + std::max(2, K / 2);
    if (ks >= N) ks = K + 1;
    return (ks >= N) ? -1 : ks;
}

// Shrink_guided interno con cap de tiempo. Agrega instancias a `out`.
static void sp_collect(int N, int K, long long n_att, int seed,
                        int max_to_add, std::vector<SInst>& out,
                        std::chrono::steady_clock::time_point t_ref, double t_cap_s) {
    int K_start = kstart_for(K, N);
    if (K_start < 0) return;

    std::mt19937 gen_rng(seed);
    std::mt19937 exp_rng(seed ^ 0xDEAD);
    int qr[MAXN], qc[MAXN];
    int added = 0;

    for (long long n_gen = 0; n_gen < n_att && added < max_to_add; n_gen++) {
        if (n_gen % 1000 == 0) {
            double el = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_ref).count();
            if (el > t_cap_s) break;
        }

        if (!gen_placement_raw(N, K_start, qr, qc, gen_rng)) continue;
        if (pipeline(qr, qc, K_start, N) != UNSAT_DET) continue;

        int cur_qr[MAXN], cur_qc[MAXN];
        memcpy(cur_qr, qr, K_start * sizeof(int));
        memcpy(cur_qc, qc, K_start * sizeof(int));
        int cur_K = K_start;

        while (cur_K > K) {
            struct Cand { double peer; int idx; };
            Cand cands[MAXN]; int nc = 0;
            for (int idx = 0; idx < cur_K; idx++) {
                int nqr[MAXN], nqc[MAXN]; int nk = 0;
                for (int j = 0; j < cur_K; j++)
                    if (j != idx) { nqr[nk] = cur_qr[j]; nqc[nk] = cur_qc[j]; nk++; }
                if (pipeline(nqr, nqc, nk, N) != UNSAT_DET) continue;
                double peer = compute_avg_peer(nqr, nqc, nk, N);
                cands[nc++] = {peer, idx};
            }
            if (nc == 0) break;
            if (nc > 1)
                std::sort(cands, cands + nc,
                          [](const Cand& a, const Cand& b) { return a.peer < b.peer; });
            int drop = cands[0].idx;
            int nqr[MAXN], nqc[MAXN]; int nk = 0;
            for (int j = 0; j < cur_K; j++)
                if (j != drop) { nqr[nk] = cur_qr[j]; nqc[nk] = cur_qc[j]; nk++; }
            memcpy(cur_qr, nqr, nk * sizeof(int));
            memcpy(cur_qc, nqc, nk * sizeof(int));
            cur_K = nk;
        }

        if (cur_K == K) {
            SInst inst;
            memcpy(inst.qr, cur_qr, K * sizeof(int));
            memcpy(inst.qc, cur_qc, K * sizeof(int));
            out.push_back(inst);
            added++;
        }
    }
}

// Resultado por N para la tabla de escalado
struct NRes {
    int N, K, n_inst, bud_max, n_esc;
    double bud_mean, t_mean_us, t_max_us, dist_ctr, spread;
};

void sweep_pipeline(int N_lo, int N_hi, int max_inst, long long n_att_base,
                     double tlim_s) {
    printf("================================================================\n");
    printf("  SWEEP_PIPELINE  N=%d..%d  max_inst=%d  tlim=%.0fs\n",
           N_lo, N_hi, max_inst, tlim_s);
    printf("  K = floor(N/4)  (confirmado empiricamente hasta N=21)\n");
    printf("  CMR: depth=%d  budget=%d\n", g_pipeline_depth, g_max_combos);
    printf("================================================================\n");
    fflush(stdout);

    auto wall_start = std::chrono::steady_clock::now();
    std::vector<NRes> all_res;

    for (int N = N_lo; N <= N_hi; N++) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - wall_start).count();
        if (elapsed > tlim_s) {
            printf("\n[limite %.0fs alcanzado, parando en N=%d]\n", tlim_s, N);
            break;
        }
        double remaining = tlim_s - elapsed;

        // K_min confirmado por kmin_sweep exhaustivo (2026-08-01)
        // N=14-15: kmin_sweep timeout en K=3, patron_N14_K4.csv confirma K=4
        // N>=16: confirmado via shrink_guided y patron CSV
        static const int kmin_table[] = {
            0,  // N=0
            0,  // N=1
            0,  // N=2
            0,  // N=3
            1,  // N=4  floor=ceil=1
            2,  // N=5  ceil(5/4)=2
            1,  // N=6  floor(6/4)=1 (caso especial)
            2,  // N=7  ceil(7/4)=2
            2,  // N=8  floor=ceil=2
            2,  // N=9  floor(9/4)=2
            3,  // N=10 ceil(10/4)=3
            3,  // N=11 ceil(11/4)=3
            3,  // N=12 floor=ceil=3
            3,  // N=13 floor(13/4)=3
            4,  // N=14 kmin_sweep sin K=3, patron_N14_K4.csv confirma K=4
            4,  // N=15 mismo patron (ceil(15/4)=4)
            4,  // N=16 confirmado por patron_N16_K4.csv
            4,  // N=17 confirmado por shrink_guided
            5,  // N=18 patron: N%4==2 -> ceil(N/4) como N=10,14
            5,  // N=19 patron: N%4==3 -> ceil(N/4) como N=11,15
        };
        int K;
        if (N < (int)(sizeof(kmin_table)/sizeof(kmin_table[0])))
            K = kmin_table[N];
        else
            K = N / 4;  // confirmado empiricamente N=20,21 (shrink_guided)
        if (K < 1) K = 1;

        printf("\n--- N=%d  K=%d  [%.0fs restantes] ---\n", N, K, remaining);
        fflush(stdout);

        std::vector<SInst> instances;

        // 1. CSV
        char fname[256];
        snprintf(fname, sizeof(fname), "datos/patron_N%d_K%d.csv", N, K);
        int csv_n = sp_load_csv(fname, K, instances, max_inst);
        if (csv_n > 0)
            printf("  CSV: %d instancias de %s\n", csv_n, fname);

        // 2. shrink_guided si faltan
        if ((int)instances.size() < std::min(max_inst, 8)) {
            // min 100k para N>=15 (necesario para encontrar instancias K_min escasas)
            long long n_att = std::max(100000LL, n_att_base * (long long)(N * N));
            // cap: hasta 120s para N grandes
            double t_cap = std::min(remaining / 2.0, 120.0);
            int Ks_show = std::min(2 * K, N - 1);
            printf("  shrink: K_start=%d att=%lld cap=%.0fs\n",
                   Ks_show, n_att, t_cap);
            fflush(stdout);

            auto t_shrink = std::chrono::steady_clock::now();
            int before = (int)instances.size();
            sp_collect(N, K, n_att, 42 + N,
                        max_inst - before, instances, t_shrink, t_cap);
            int new_found = (int)instances.size() - before;
            double dt_s = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_shrink).count();
            printf("  shrink: +%d instancias en %.1fs\n", new_found, dt_s);
        }

        if (instances.empty()) {
            printf("  SIN INSTANCIAS\n");
            continue;
        }

        int n_inst = std::min((int)instances.size(), max_inst);
        double center = (N - 1) / 2.0;

        // CMR + geometria
        double sum_bud = 0, sum_t = 0, sum_dist = 0, sum_spread = 0, max_t = 0;
        int bud_max = 0, n_esc = 0;
        std::map<int, int> bud_hist;

        for (int i = 0; i < n_inst; i++) {
            SInst& inst = instances[i];

            // Geometria
            double cr = 0, cc = 0;
            for (int j = 0; j < K; j++) { cr += inst.qr[j]; cc += inst.qc[j]; }
            cr /= K; cc /= K;
            double dist = sqrt((cr - center) * (cr - center) +
                               (cc - center) * (cc - center));
            double vr = 0, vc = 0;
            for (int j = 0; j < K; j++) {
                vr += (inst.qr[j] - cr) * (inst.qr[j] - cr);
                vc += (inst.qc[j] - cc) * (inst.qc[j] - cc);
            }
            double spread = K > 1 ? (sqrt(vr / K) + sqrt(vc / K)) / 2.0 : 0.0;
            sum_dist   += dist;
            sum_spread += spread;

            // CMR con timing
            auto t0 = std::chrono::steady_clock::now();
            CMRResult r = cmr_analyze(inst.qr, inst.qc, K, N);
            auto t1 = std::chrono::steady_clock::now();
            double dt_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

            int bud;
            if      (r.detect_budget == 0) bud = 0;
            else if (r.detect_budget  > 0) bud = r.detect_budget;
            else { bud = 999999; n_esc++; }

            if (bud < 999999) {
                bud_hist[bud]++;
                sum_bud += bud;
                if (bud > bud_max) bud_max = bud;
            }
            sum_t += dt_us;
            if (dt_us > max_t) max_t = dt_us;
        }

        int n_valid = n_inst - n_esc;
        double bud_mean   = n_valid > 0 ? sum_bud / n_valid : 0;
        double t_mean_us  = n_inst  > 0 ? sum_t   / n_inst  : 0;
        double dist_mean  = sum_dist   / n_inst;
        double spr_mean   = sum_spread / n_inst;

        printf("  CMR: n=%d  bud_max=%d  bud_mean=%.1f  "
               "t_mean=%.1fus  t_max=%.1fus  esc=%d\n",
               n_inst, bud_max, bud_mean, t_mean_us, max_t, n_esc);
        printf("  GEO: dist_ctr=%.2f (%.3f/N)  spread=%.2f (%.3f/N)\n",
               dist_mean, dist_mean / N, spr_mean, spr_mean / N);
        printf("  BUD: {");
        for (auto& [b, c] : bud_hist) printf("%d:%d ", b, c);
        printf("}\n");
        fflush(stdout);

        all_res.push_back({N, K, n_inst, bud_max, n_esc,
                            bud_mean, t_mean_us, max_t, dist_mean, spr_mean});
    }

    // ── Tabla de escalado ────────────────────────────────────────────────────
    printf("\n================================================================\n");
    printf("  TABLA DE ESCALADO\n");
    printf("  %-4s %-3s %-6s %-7s %-8s %-12s %-10s %-8s %-8s %s\n",
           "N", "K", "n_inst", "bud_max", "bud_mean",
           "t_mean_us", "t_max_us", "dist/N", "spr/N", "esc");
    printf("  %s\n", std::string(90, '-').c_str());
    for (auto& r : all_res) {
        printf("  %-4d %-3d %-6d %-7d %-8.1f %-12.1f %-10.1f %-8.4f %-8.4f %d\n",
               r.N, r.K, r.n_inst, r.bud_max, r.bud_mean,
               r.t_mean_us, r.t_max_us, r.dist_ctr / r.N, r.spread / r.N, r.n_esc);
    }

    // ── Regresion log-log t ~ N^alpha ────────────────────────────────────────
    {
        double sx = 0, sy = 0, sxy = 0, sx2 = 0; int np = 0;
        for (auto& r : all_res) {
            if (r.t_mean_us <= 0) continue;
            double lx = log((double)r.N), ly = log(r.t_mean_us);
            sx += lx; sy += ly; sxy += lx * ly; sx2 += lx * lx; np++;
        }
        if (np >= 4) {
            double alpha = (np * sxy - sx * sy) / (np * sx2 - sx * sx);
            printf("\n  Regresion log-log: t_mean ~ N^%.2f\n", alpha);
            if      (alpha < 1.5) printf("  -> SUBLINEAL O(N^%.1f)\n", alpha);
            else if (alpha < 2.5) printf("  -> CUADRATICO O(N^%.1f)\n", alpha);
            else if (alpha < 3.5) printf("  -> CUBICO O(N^%.1f)\n", alpha);
            else                  printf("  -> SUPERCUBICO O(N^%.1f)\n", alpha);
        }
    }

    // ── Patron geometrico: tendencia dist/N con N ────────────────────────────
    printf("\n  PATRON GEOMETRICO: dist/N vs N\n");
    printf("  (dist/N->0 = centrado; dist/N->0.5 = esquinas)\n");
    printf("  %-4s %-3s  dist/N  spr/N  tag\n", "N", "K");
    for (auto& r : all_res) {
        double dn = r.dist_ctr / r.N;
        double sn = r.spread / r.N;
        const char* tag = (dn < 0.08) ? "CENTRAL" :
                          (dn < 0.20) ? "semi-central" : "distribuido";
        printf("  %-4d %-3d  %.4f  %.4f  %s\n", r.N, r.K, dn, sn, tag);
    }

    double total_s = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - wall_start).count();
    printf("\n  Tiempo total: %.2fs\n", total_s);
    printf("================================================================\n");
}

// ─── GEO_STAT ─────────────────────────────────────────────────────────────────
// Verifica: UNSAT K_min -> reinas centrales -> alta cobertura diagonal -> bajo avg_peer.
// Corre para N_lo..N_hi. Muestra gap_peer, gap_cov, corr(cov,peer), tendencia con N.

static double geo_diag_coverage(const int* qr, const int* qc, int K, int N) {
    double total = 0;
    for (int i = 0; i < K; i++) {
        int r = qr[i], c = qc[i];
        int D = std::min(r,c) + std::min(N-1-r, N-1-c)
              + std::min(r, N-1-c) + std::min(N-1-r, c);
        total += D;
    }
    return total / K / (2.0 * (N - 1));
}

// col_spread: std de las columnas de todas las celdas viables en filas libres.
// UNSAT hipotesis: col_spread bajo (celdas agrupadas en cols centrales) -> peer bajo.
static double geo_col_spread(const int* qr, const int* qc, int K, int N) {
    bool placed[MAXN] = {};
    for (int i = 0; i < K; i++) placed[qr[i]] = true;
    int NF = 0;
    int fr[MAXN]; bitmask dom[MAXN];
    for (int r = 0; r < N; r++) {
        if (!placed[r]) { dom[NF] = available_bits(r, N, qr, qc, K); fr[NF++] = r; }
    }
    if (NF == 0) return 0.0;
    long long sum_c = 0, sum_c2 = 0, cnt = 0;
    for (int i = 0; i < NF; i++) {
        bitmask d = dom[i];
        while (d) { int c = ctz_bm(d); d &= d-1; sum_c += c; sum_c2 += c*c; cnt++; }
    }
    if (cnt < 2) return 0.0;
    double mean = (double)sum_c / cnt;
    double var  = (double)sum_c2 / cnt - mean * mean;
    return sqrt(var > 0 ? var : 0) / (N - 1); // normalizado por N-1
}

// col_share: fraccion media de columnas viables compartidas entre pares de filas libres.
// MECANISMO HIPOTESIS: reinas centrales bloquean mismas cols en todas las filas
// -> viables restantes se solapan -> conflictos cc altos -> avg_peer bajo.
static double geo_col_share(const int* qr, const int* qc, int K, int N) {
    bool placed[MAXN] = {};
    for (int i = 0; i < K; i++) placed[qr[i]] = true;
    int NF = 0;
    int fr[MAXN]; bitmask dom[MAXN];
    for (int r = 0; r < N; r++) {
        if (!placed[r]) { dom[NF] = available_bits(r, N, qr, qc, K); fr[NF++] = r; }
    }
    if (NF < 2) return 0.0;
    long long total_share = 0, total_min = 0, npairs = 0;
    for (int i = 0; i < NF; i++) {
        int pi = pop_bm(dom[i]);
        for (int j = i+1; j < NF; j++) {
            int pj  = pop_bm(dom[j]);
            int pij = pop_bm(dom[i] & dom[j]);
            int mn  = std::min(pi, pj);
            if (mn > 0) { total_share += pij; total_min += mn; npairs++; }
        }
    }
    return npairs > 0 ? (double)total_share / total_min : 0.0;
}

// mean_dom: celdas viables promedio por fila libre = TV / (N-K).
// Si UNSAT tiene mean_dom << SAT -> ese es el mecanismo raiz (dominios pequenos + conflictos diag)
static double geo_mean_dom(const int* qr, const int* qc, int K, int N) {
    bool placed[MAXN] = {};
    for (int i=0;i<K;i++) placed[qr[i]]=true;
    int NF=0; long long TV=0;
    for (int r=0;r<N;r++) {
        if (!placed[r]) { TV += pop_bm(available_bits(r, N, qr, qc, K)); NF++; }
    }
    return NF>0 ? (double)TV/NF : 0.0;
}

struct GeoInst { double avg_peer, coverage, dist_ctr, col_spread, col_share, mean_dom; bool is_unsat; };

// Computa avg_peer + col_spread + col_share + mean_dom en un solo pase sobre dom[].
// Evita 4 llamadas independientes que reconstruyen placed[]+dom[] cada una.
static void geo_stats_all(const int* qr, const int* qc, int K, int N,
                           double& out_peer, double& out_csprd,
                           double& out_csh, double& out_mdom) {
    bool placed[MAXN] = {};
    for (int i = 0; i < K; i++) placed[qr[i]] = true;
    int fr[MAXN]; bitmask dom[MAXN]; int NF = 0;
    for (int r = 0; r < N; r++)
        if (!placed[r]) { dom[NF] = available_bits(r, N, qr, qc, K); fr[NF++] = r; }

    if (NF < 2) { out_peer = out_csprd = out_csh = out_mdom = 0.0; return; }

    // mean_dom: TV/NF
    long long TV = 0, sq_sum = 0;
    for (int i = 0; i < NF; i++) { long long p = pop_bm(dom[i]); TV += p; sq_sum += p*p; }
    out_mdom = (double)TV / NF;

    if (TV < 2) { out_peer = out_csprd = out_csh = out_mdom = 0.0; return; }

    // avg_peer: misma fórmula que compute_avg_peer
    long long conflicts = 0;
    // col_share + col_spread acumuladores
    long long sum_c = 0, sum_c2 = 0, cnt_c = 0;
    long long total_share = 0, total_min = 0, npairs = 0;

    for (int i = 0; i < NF; i++) {
        // col_spread acumuladores
        bitmask d = dom[i];
        while (d) { int c = ctz_bm(d); d &= d-1; sum_c += c; sum_c2 += c*c; cnt_c++; }

        for (int j = i + 1; j < NF; j++) {
            int diff = fr[j] - fr[i];
            // avg_peer conflicts
            long long cc = pop_bm(dom[i] & dom[j]);
            long long dc = pop_bm(dom[i] & (dom[j] >> diff))
                         + pop_bm(dom[i] & (dom[j] << diff));
            conflicts += 2 * (cc + dc);
            // col_share
            int pi = pop_bm(dom[i]), pj = pop_bm(dom[j]);
            int pij = (int)cc;
            int mn = std::min(pi, pj);
            if (mn > 0) { total_share += pij; total_min += mn; npairs++; }
        }
    }
    out_peer  = (double)(TV*TV - sq_sum - conflicts) / TV / (TV - 1);
    double mean_c = (cnt_c > 0) ? (double)sum_c/cnt_c : 0.0;
    double var_c  = (cnt_c > 1) ? (double)sum_c2/cnt_c - mean_c*mean_c : 0.0;
    out_csprd = sqrt(var_c > 0 ? var_c : 0) / (N - 1);
    out_csh   = (npairs > 0) ? (double)total_share / total_min : 0.0;
}

void geo_stat(int N_lo, int N_hi, int n_unsat, int n_sat, long long n_att, int seed) {
    printf("================================================================\n");
    printf("  GEO_STAT  N=%d..%d  n_unsat=%d  n_sat=%d\n", N_lo, N_hi, n_unsat, n_sat);
    printf("  Hipotesis: UNSAT->central->alta_cobertura->bajo_avg_peer\n");
    printf("================================================================\n");
    fflush(stdout);

    auto wall_start = std::chrono::steady_clock::now();

    struct NEffect {
        int N, K, nu, ns;
        double peer_u, peer_s, mdom_u, mdom_s, dist_u, dist_s, share_u, share_s, corr;
        double max_pu, min_ps, max_mdu, min_mds;
        bool separated, sep_dom;
    };
    std::vector<NEffect> efects;

    for (int N = N_lo; N <= N_hi; N++) {
        int K = get_kmin(N);
        printf("\n--- N=%d  K=%d ---\n", N, K);
        fflush(stdout);

        std::vector<GeoInst> inst;
        double center = (N - 1) / 2.0;

        // UNSAT: CSV o shrink_guided
        {
            std::vector<SInst> raw;
            char fname[256];
            snprintf(fname, sizeof(fname), "datos/patron_N%d_K%d.csv", N, K);
            sp_load_csv(fname, K, raw, n_unsat);
            if ((int)raw.size() < n_unsat) {
                auto tref = std::chrono::steady_clock::now();
                sp_collect(N, K, n_att, seed + N, n_unsat - (int)raw.size(),
                           raw, tref, 90.0);
            }
            for (auto& x : raw) {
                double peer, csprd, csh, mdom;
                geo_stats_all(x.qr, x.qc, K, N, peer, csprd, csh, mdom);
                double cov = geo_diag_coverage(x.qr, x.qc, K, N);
                double cr=0, cc=0;
                for (int j=0;j<K;j++){cr+=x.qr[j];cc+=x.qc[j];}
                cr/=K; cc/=K;
                double dist = sqrt((cr-center)*(cr-center)+(cc-center)*(cc-center))/N;
                inst.push_back({peer, cov, dist, csprd, csh, mdom, true});
            }
            printf("  UNSAT: %d instancias\n", (int)raw.size());
        }

        // SAT: placements aleatorios que NO detecta el pipeline como UNSAT
        {
            std::mt19937 rng(seed ^ 0xABCD ^ N);
            int qr[MAXN], qc[MAXN];
            int added = 0;
            long long tries = 0, max_tries = n_att * 20;
            while (added < n_sat && tries < max_tries) {
                tries++;
                if (!gen_placement_raw(N, K, qr, qc, rng)) continue;
                if (pipeline(qr, qc, K, N) == UNSAT_DET) continue;
                double peer, csprd, csh, mdom;
                geo_stats_all(qr, qc, K, N, peer, csprd, csh, mdom);
                double cov = geo_diag_coverage(qr, qc, K, N);
                double cr=0, cc=0;
                for (int j=0;j<K;j++){cr+=qr[j];cc+=qc[j];}
                cr/=K; cc/=K;
                double dist = sqrt((cr-center)*(cr-center)+(cc-center)*(cc-center))/N;
                inst.push_back({peer, cov, dist, csprd, csh, mdom, false});
                added++;
            }
            printf("  SAT:   %d instancias (%lld intentos)\n", added, tries);
        }

        double spu=0,scu=0,sdu=0,ssu=0,sqsu=0,smdu=0; int nu=0;
        double sps=0,scs=0,sds=0,sss=0,sqss=0,smds=0; int ns=0;
        for (auto& g : inst) {
            if (g.is_unsat){spu+=g.avg_peer;scu+=g.coverage;sdu+=g.dist_ctr;ssu+=g.col_spread;sqsu+=g.col_share;smdu+=g.mean_dom;nu++;}
            else            {sps+=g.avg_peer;scs+=g.coverage;sds+=g.dist_ctr;sss+=g.col_spread;sqss+=g.col_share;smds+=g.mean_dom;ns++;}
        }
        if (!nu || !ns) { printf("  SIN DATOS\n"); continue; }

        double mpu=spu/nu, mcu=scu/nu, mdu=sdu/nu, msu=ssu/nu, mshu=sqsu/nu, mmdu=smdu/nu;
        double mps=sps/ns, mcs=scs/ns, mds=sds/ns, mss=sss/ns, mshs=sqss/ns, mmds=smds/ns;

        // Correlaciones: col_share, col_spread, coverage con avg_peer
        int np=(int)inst.size();
        auto pearson = [&](auto xfn, auto yfn) {
            double sx=0,sy=0,sx2=0,sy2=0,sxy=0;
            for (auto& g : inst) {
                double x=xfn(g), y=yfn(g);
                sx+=x;sy+=y;sx2+=x*x;sy2+=y*y;sxy+=x*y;
            }
            double d=(np*sx2-sx*sx)*(np*sy2-sy*sy);
            return d>0 ? (np*sxy-sx*sy)/sqrt(d) : 0.0;
        };
        auto peer_fn = [](const GeoInst& g){ return g.avg_peer; };
        double corr_sh = pearson([](const GeoInst& g){ return g.col_share; }, peer_fn);
        double corr_md = pearson([](const GeoInst& g){ return g.mean_dom;  }, peer_fn);
        double corr_cv = pearson([](const GeoInst& g){ return g.coverage;  }, peer_fn);

        double max_pu=-1, min_ps=1e9;
        double max_mdu=-1, min_mds=1e9;
        for (auto& g : inst) {
            if (g.is_unsat) {
                if (g.avg_peer > max_pu)  max_pu  = g.avg_peer;
                if (g.mean_dom > max_mdu) max_mdu = g.mean_dom;
            } else {
                if (g.avg_peer < min_ps)  min_ps  = g.avg_peer;
                if (g.mean_dom < min_mds) min_mds = g.mean_dom;
            }
        }
        bool sep     = (max_pu  < min_ps);
        bool sep_dom = (max_mdu < min_mds);

        printf("  PEER:    UNSAT=%.5f  SAT=%.5f  gap=%+.5f  max_U=%.5f min_S=%.5f [%s]\n",
               mpu, mps, mps-mpu, max_pu, min_ps, sep?"SEP":"overlap");
        printf("  MEAN_DOM:UNSAT=%.3f  SAT=%.3f  gap=%+.3f  max_U=%.3f min_S=%.3f [%s]\n",
               mmdu, mmds, mmdu-mmds, max_mdu, min_mds, sep_dom?"SEP":"overlap");
        printf("  SHARE:   UNSAT=%.4f  SAT=%.4f  gap=%+.4f\n", mshu, mshs, mshu-mshs);
        printf("  COV:     UNSAT=%.4f  SAT=%.4f  gap=%+.4f  dist_U=%.4f dist_S=%.4f\n",
               mcu, mcs, mcu-mcs, mdu, mds);
        printf("  CORR(mdom,peer)=%+.4f  CORR(share,peer)=%+.4f  CORR(cov,peer)=%+.4f\n",
               corr_md, corr_sh, corr_cv);
        fflush(stdout);

        efects.push_back({N,K,nu,ns,mpu,mps,mmdu,mmds,mdu,mds,mshu,mshs,corr_md,
                           max_pu,min_ps,max_mdu,min_mds,sep,sep_dom});
    }

    printf("\n================================================================\n");
    printf("  TABLA RESUMEN\n");
    printf("  %-3s K  nu   ns   peer_U  peer_S  gap_peer  mdom_U mdom_S gap_dom  share_gap  dist_U dist_S  sep\n","N");
    printf("  %s\n", std::string(110,'-').c_str());
    for (auto& e : efects) {
        printf("  %-3d %d  %-4d %-4d  %.5f %.5f %+.5f  %.2f   %.2f   %+.2f    %+.4f    %.4f %.4f  %s\n",
               e.N, e.K, e.nu, e.ns,
               e.peer_u, e.peer_s, e.peer_s-e.peer_u,
               e.mdom_u, e.mdom_s, e.mdom_u-e.mdom_s,
               e.share_u-e.share_s,
               e.dist_u, e.dist_s,
               e.separated?"SI":"NO");
    }

    printf("\n  COMPARACION: separacion peer vs mean_dom\n");
    printf("  %-3s K  gap_peer  max_pu  min_ps  sep_peer   gap_dom  max_mdu min_mds  sep_dom\n","N");
    printf("  %s\n", std::string(88,'-').c_str());
    for (auto& e : efects)
        printf("  %-3d %d  %+.5f  %.5f %.5f  %-6s    %+.3f   %.3f  %.3f    %-6s\n",
               e.N, e.K, e.peer_s-e.peer_u, e.max_pu, e.min_ps,
               e.separated?"SI":"NO",
               e.mdom_s-e.mdom_u, e.max_mdu, e.min_mds,
               e.sep_dom?"SI":"NO");

    double total_s = std::chrono::duration<double>(
        std::chrono::steady_clock::now()-wall_start).count();
    printf("\n  Tiempo total: %.2fs\n", total_s);
    printf("================================================================\n");
}

// ─── FAST_KMIN ────────────────────────────────────────────────────────────────
// Búsqueda directa K_min UNSAT sin shrink ni pipeline previo.
// Filtro: mean_dom O(NK) < threshold → candidato UNSAT → verifica con pipeline.
// threshold=0 → usa 0.42*N automático.
void fast_kmin(int N, int K, double threshold, int n_target,
               long long n_att, int seed, bool do_verify) {
    if (threshold <= 0.0) threshold = 0.42 * N;

    printf("================================================================\n");
    printf("  FAST_KMIN  N=%d  K=%d  threshold=%.3f  n_target=%d\n",
           N, K, threshold, n_target);
    printf("  n_att=%lld  seed=%d  verify=%s\n", n_att, seed, do_verify?"SI":"NO");
    printf("  Filtro: mdom < %.3f → pipeline → UNSAT\n", threshold);
    printf("================================================================\n");
    fflush(stdout);

    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    long long n_gen = 0, n_valid = 0, n_pass = 0, n_found = 0;
    auto t_start = std::chrono::steady_clock::now();

    while (n_found < (long long)n_target && n_gen < n_att) {
        n_gen++;

        if (n_gen % 500000 == 0) {
            double el = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_start).count();
            printf("  [%lld att | %.1fs | pass_filter=%lld | found=%lld]\n",
                   n_gen, el, n_pass, n_found);
            fflush(stdout);
        }

        if (!gen_placement_raw(N, K, qr, qc, rng)) continue;
        n_valid++;

        double md = geo_mean_dom(qr, qc, K, N);
        if (md >= threshold) continue;
        n_pass++;

        if (do_verify && pipeline(qr, qc, K, N) != UNSAT_DET) continue;

        n_found++;
        printf("  UNSAT #%lld  mdom=%.4f  queens:", n_found, md);
        for (int i = 0; i < K; i++) printf(" (%d,%d)", qr[i], qc[i]);
        printf("\n");
        fflush(stdout);
    }

    double total_s = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();

    printf("\n--- RESULTADOS N=%d K=%d threshold=%.3f ---\n", N, K, threshold);
    printf("  Intentos         : %lld\n", n_gen);
    printf("  Validos (no-att) : %lld\n", n_valid);
    printf("  Pasaron filtro   : %lld  (%.5f%%)\n",
           n_pass, n_valid > 0 ? 100.0*n_pass/n_valid : 0.0);
    printf("  UNSAT encontrados: %lld\n", n_found);
    if (n_gen > 0)
        printf("  Tasa (1 en)      : %.0f intentos/UNSAT\n",
               n_found > 0 ? (double)n_gen/n_found : (double)n_gen+1.0);
    printf("  Tiempo total     : %.2fs\n", total_s);
    if (n_found > 0 && total_s > 0)
        printf("  Inst/seg         : %.2f\n", n_found/total_s);
    printf("================================================================\n");
}

// ─── SHRINK VARIANTES (comparación de estrategias) ────────────────────────────

// B: shrink con mean_dom como criterio greedy (en vez de avg_peer)
static void sp_collect_mdom(int N, int K, long long n_att, int seed,
                             int max_to_add, std::vector<SInst>& out,
                             std::chrono::steady_clock::time_point t_ref, double t_cap_s) {
    int K_start = kstart_for(K, N);
    if (K_start < 0) return;
    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    int added = 0;
    for (long long n_gen = 0; n_gen < n_att && added < max_to_add; n_gen++) {
        if (n_gen % 1000 == 0) {
            if (std::chrono::duration<double>(
                    std::chrono::steady_clock::now()-t_ref).count() > t_cap_s) break;
        }
        if (!gen_placement_raw(N, K_start, qr, qc, rng)) continue;
        if (pipeline(qr, qc, K_start, N) != UNSAT_DET) continue;
        int cur_qr[MAXN], cur_qc[MAXN];
        memcpy(cur_qr, qr, K_start*sizeof(int));
        memcpy(cur_qc, qc, K_start*sizeof(int));
        int cur_K = K_start;
        while (cur_K > K) {
            struct Cand { double score; int idx; };
            Cand cands[MAXN]; int nc = 0;
            for (int idx = 0; idx < cur_K; idx++) {
                int xqr[MAXN], xqc[MAXN]; int xk = 0;
                for (int j = 0; j < cur_K; j++)
                    if (j != idx) { xqr[xk]=cur_qr[j]; xqc[xk]=cur_qc[j]; xk++; }
                if (pipeline(xqr, xqc, xk, N) != UNSAT_DET) continue;
                cands[nc++] = {geo_mean_dom(xqr, xqc, xk, N), idx};
            }
            if (nc == 0) break;
            if (nc > 1)
                std::sort(cands, cands+nc, [](const Cand&a, const Cand&b){
                    return a.score < b.score; });
            int drop = cands[0].idx;
            int nqr[MAXN], nqc[MAXN]; int nk = 0;
            for (int j = 0; j < cur_K; j++)
                if (j != drop) { nqr[nk]=cur_qr[j]; nqc[nk]=cur_qc[j]; nk++; }
            memcpy(cur_qr,nqr,nk*sizeof(int));
            memcpy(cur_qc,nqc,nk*sizeof(int));
            cur_K = nk;
        }
        if (cur_K == K) {
            SInst inst;
            memcpy(inst.qr, cur_qr, K*sizeof(int));
            memcpy(inst.qc, cur_qc, K*sizeof(int));
            out.push_back(inst); added++;
        }
    }
}

// C: pre-filtro mean_dom en K_start antes del pipeline.
// Si mean_dom(K_start) > threshold → probable SAT → skip pipeline (caro).
// threshold=0 → auto: 0.42*N*(N-K_start)/(N-K)
static void sp_collect_prefilter(int N, int K, long long n_att, int seed,
                                  int max_to_add, std::vector<SInst>& out,
                                  std::chrono::steady_clock::time_point t_ref, double t_cap_s,
                                  double thr_kstart) {
    int K_start = kstart_for(K, N);
    if (K_start < 0) return;
    if (thr_kstart <= 0)
        thr_kstart = 0.42 * N * (N - K_start) / (double)(N - K);
    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    int added = 0;
    for (long long n_gen = 0; n_gen < n_att && added < max_to_add; n_gen++) {
        if (n_gen % 1000 == 0) {
            if (std::chrono::duration<double>(
                    std::chrono::steady_clock::now()-t_ref).count() > t_cap_s) break;
        }
        if (!gen_placement_raw(N, K_start, qr, qc, rng)) continue;
        if (geo_mean_dom(qr, qc, K_start, N) > thr_kstart) continue; // pre-filtro O(NK)
        if (pipeline(qr, qc, K_start, N) != UNSAT_DET) continue;
        int cur_qr[MAXN], cur_qc[MAXN];
        memcpy(cur_qr, qr, K_start*sizeof(int));
        memcpy(cur_qc, qc, K_start*sizeof(int));
        int cur_K = K_start;
        while (cur_K > K) {
            struct Cand { double peer; int idx; };
            Cand cands[MAXN]; int nc = 0;
            for (int idx = 0; idx < cur_K; idx++) {
                int xqr[MAXN], xqc[MAXN]; int xk = 0;
                for (int j = 0; j < cur_K; j++)
                    if (j != idx) { xqr[xk]=cur_qr[j]; xqc[xk]=cur_qc[j]; xk++; }
                if (pipeline(xqr, xqc, xk, N) != UNSAT_DET) continue;
                cands[nc++] = {compute_avg_peer(xqr, xqc, xk, N), idx};
            }
            if (nc == 0) break;
            if (nc > 1)
                std::sort(cands, cands+nc, [](const Cand&a, const Cand&b){
                    return a.peer < b.peer; });
            int drop = cands[0].idx;
            int nqr[MAXN], nqc[MAXN]; int nk = 0;
            for (int j = 0; j < cur_K; j++)
                if (j != drop) { nqr[nk]=cur_qr[j]; nqc[nk]=cur_qc[j]; nk++; }
            memcpy(cur_qr,nqr,nk*sizeof(int));
            memcpy(cur_qc,nqc,nk*sizeof(int));
            cur_K = nk;
        }
        if (cur_K == K) {
            SInst inst;
            memcpy(inst.qr, cur_qr, K*sizeof(int));
            memcpy(inst.qc, cur_qc, K*sizeof(int));
            out.push_back(inst); added++;
        }
    }
}

// D: generación K_start con sesgo central.
// Rechaza colocaciones cuyo centroide diste más de max_d*N del centro del tablero.
// max_d=0 → default 0.15 (15% de N)
static void sp_collect_central(int N, int K, long long n_att, int seed,
                                int max_to_add, std::vector<SInst>& out,
                                std::chrono::steady_clock::time_point t_ref, double t_cap_s,
                                double max_d) {
    int K_start = kstart_for(K, N);
    if (K_start < 0) return;
    if (max_d <= 0) max_d = 0.15;
    double center = (N - 1) / 2.0;
    double max_dist = max_d * N;
    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    int added = 0;
    for (long long n_gen = 0; n_gen < n_att && added < max_to_add; n_gen++) {
        if (n_gen % 1000 == 0) {
            if (std::chrono::duration<double>(
                    std::chrono::steady_clock::now()-t_ref).count() > t_cap_s) break;
        }
        if (!gen_placement_raw(N, K_start, qr, qc, rng)) continue;
        // Filtro central: centroide del K_start placement
        double cr=0, cc=0;
        for (int i=0; i<K_start; i++) { cr+=qr[i]; cc+=qc[i]; }
        cr/=K_start; cc/=K_start;
        double dist = std::sqrt((cr-center)*(cr-center)+(cc-center)*(cc-center));
        if (dist > max_dist) continue;
        if (pipeline(qr, qc, K_start, N) != UNSAT_DET) continue;
        int cur_qr[MAXN], cur_qc[MAXN];
        memcpy(cur_qr, qr, K_start*sizeof(int));
        memcpy(cur_qc, qc, K_start*sizeof(int));
        int cur_K = K_start;
        while (cur_K > K) {
            struct Cand { double peer; int idx; };
            Cand cands[MAXN]; int nc = 0;
            for (int idx = 0; idx < cur_K; idx++) {
                int xqr[MAXN], xqc[MAXN]; int xk = 0;
                for (int j = 0; j < cur_K; j++)
                    if (j != idx) { xqr[xk]=cur_qr[j]; xqc[xk]=cur_qc[j]; xk++; }
                if (pipeline(xqr, xqc, xk, N) != UNSAT_DET) continue;
                cands[nc++] = {compute_avg_peer(xqr, xqc, xk, N), idx};
            }
            if (nc == 0) break;
            if (nc > 1)
                std::sort(cands, cands+nc, [](const Cand&a, const Cand&b){
                    return a.peer < b.peer; });
            int drop = cands[0].idx;
            int nqr[MAXN], nqc[MAXN]; int nk = 0;
            for (int j = 0; j < cur_K; j++)
                if (j != drop) { nqr[nk]=cur_qr[j]; nqc[nk]=cur_qc[j]; nk++; }
            memcpy(cur_qr,nqr,nk*sizeof(int));
            memcpy(cur_qc,nqc,nk*sizeof(int));
            cur_K = nk;
        }
        if (cur_K == K) {
            SInst inst;
            memcpy(inst.qr, cur_qr, K*sizeof(int));
            memcpy(inst.qc, cur_qc, K*sizeof(int));
            out.push_back(inst); added++;
        }
    }
}

// ─── OPCIÓN E: Generación greedy de K_start por mean_dom ─────────────────────
// Construye K_start queens paso a paso: en cada paso prueba todas las posiciones
// válidas, computa mean_dom de cada una, y muestrea entre las top_k mejores.
// top_k controla aleatoriedad: top_k=1 determinista, top_k=N exploratorio.
static bool gen_placement_greedy_mdom(int N, int K_start, int* qr, int* qc,
                                       std::mt19937& rng, int top_k) {
    for (int k = 0; k < K_start; k++) {
        struct Cand { double md; int r, c; };
        std::vector<Cand> cands;
        cands.reserve(N * N / 2);

        bool row_used[MAXN] = {};
        for (int i = 0; i < k; i++) row_used[qr[i]] = true;

        // Array temporal: k queens colocadas + candidato en posición k
        int tqr[MAXN], tqc[MAXN];
        memcpy(tqr, qr, k * sizeof(int));
        memcpy(tqc, qc, k * sizeof(int));

        for (int r = 0; r < N; r++) {
            if (row_used[r]) continue;
            uint64_t avail = ((uint64_t)1 << N) - 1;
            for (int i = 0; i < k; i++) {
                avail &= ~((uint64_t)1 << qc[i]);
                int diff = r - qr[i];
                if (qc[i]+diff >= 0 && qc[i]+diff < N) avail &= ~((uint64_t)1 << (qc[i]+diff));
                if (qc[i]-diff >= 0 && qc[i]-diff < N) avail &= ~((uint64_t)1 << (qc[i]-diff));
            }
            uint64_t tmp = avail;
            while (tmp) {
                int c = __builtin_ctzll(tmp); tmp &= tmp-1;
                tqr[k] = r; tqc[k] = c;
                cands.push_back({geo_mean_dom(tqr, tqc, k+1, N), r, c});
            }
        }

        if (cands.empty()) return false;

        // Ordenar ascendente y muestrear de los top_k mejores
        std::sort(cands.begin(), cands.end(),
                  [](const Cand& a, const Cand& b){ return a.md < b.md; });
        int pick_n = std::min(top_k, (int)cands.size());
        std::uniform_int_distribution<int> ud(0, pick_n - 1);
        int idx = ud(rng);
        qr[k] = cands[idx].r;
        qc[k] = cands[idx].c;
    }
    return true;
}

static void sp_collect_greedy(int N, int K, long long n_att, int seed,
                               int max_to_add, std::vector<SInst>& out,
                               std::chrono::steady_clock::time_point t_ref, double t_cap_s,
                               int top_k, int K_start_override = 0) {
    int K_start = K_start_override > 0 ? K_start_override : kstart_for(K, N);
    if (K_start < 0 || K_start >= N) return;
    if (top_k <= 0) top_k = 5;
    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    int added = 0;
    for (long long n_gen = 0; n_gen < n_att && added < max_to_add; n_gen++) {
        if (n_gen % 50 == 0) {
            if (std::chrono::duration<double>(
                    std::chrono::steady_clock::now()-t_ref).count() > t_cap_s) break;
        }
        if (!gen_placement_greedy_mdom(N, K_start, qr, qc, rng, top_k)) continue;
        if (pipeline(qr, qc, K_start, N) != UNSAT_DET) continue;
        int cur_qr[MAXN], cur_qc[MAXN];
        memcpy(cur_qr, qr, K_start*sizeof(int));
        memcpy(cur_qc, qc, K_start*sizeof(int));
        int cur_K = K_start;
        while (cur_K > K) {
            struct Cand { double peer; int idx; };
            Cand cands[MAXN]; int nc = 0;
            for (int i = 0; i < cur_K; i++) {
                int xqr[MAXN], xqc[MAXN]; int xk = 0;
                for (int j = 0; j < cur_K; j++)
                    if (j != i) { xqr[xk]=cur_qr[j]; xqc[xk]=cur_qc[j]; xk++; }
                if (pipeline(xqr, xqc, xk, N) != UNSAT_DET) continue;
                cands[nc++] = {compute_avg_peer(xqr, xqc, xk, N), i};
            }
            if (nc == 0) break;
            if (nc > 1)
                std::sort(cands, cands+nc, [](const Cand&a, const Cand&b){
                    return a.peer < b.peer; });
            int drop = cands[0].idx;
            int nqr[MAXN], nqc[MAXN]; int nk = 0;
            for (int j = 0; j < cur_K; j++)
                if (j != drop) { nqr[nk]=cur_qr[j]; nqc[nk]=cur_qc[j]; nk++; }
            memcpy(cur_qr,nqr,nk*sizeof(int));
            memcpy(cur_qc,nqc,nk*sizeof(int));
            cur_K = nk;
        }
        if (cur_K == K) {
            SInst inst;
            memcpy(inst.qr, cur_qr, K*sizeof(int));
            memcpy(inst.qc, cur_qc, K*sizeof(int));
            out.push_back(inst); added++;
        }
    }
}

// ─── OPCIÓN F: Greedy rápido con estado incremental ───────────────────────────
// Igual que gen_placement_greedy_mdom pero:
// 1. Mantiene avail[r] y TV como estado acumulado → evaluación O(N) por candidato
// 2. Muestrea max_sample candidatos por paso en vez de evaluarlos todos
// Resultado: ~50-100x más rápido que la versión lenta en N grande.
static bool gen_placement_greedy_fast(int N, int K_start, int* qr, int* qc,
                                       std::mt19937& rng, int top_k, int max_sample) {
    if (max_sample <= 0) max_sample = 80;

    // Estado incremental: avail[r] = columnas disponibles en fila r
    uint64_t avail[MAXN];
    int      tv[MAXN];       // popcount(avail[r])
    const uint64_t full = ((uint64_t)1 << N) - 1;
    for (int r = 0; r < N; r++) { avail[r] = full; tv[r] = N; }
    bool row_used[MAXN] = {};
    long long TV = (long long)N * N;
    int NF = N;

    for (int k = 0; k < K_start; k++) {
        // Recoger todos los candidatos válidos
        struct Pos { int r, c; };
        std::vector<Pos> pool;
        pool.reserve(N * N / 2);
        for (int r = 0; r < N; r++) {
            if (row_used[r]) continue;
            uint64_t tmp = avail[r];
            while (tmp) { int c = __builtin_ctzll(tmp); tmp &= tmp-1; pool.push_back({r,c}); }
        }
        if (pool.empty()) return false;

        // Muestrear max_sample sin reemplazo (Fisher-Yates parcial)
        int n_s = std::min(max_sample, (int)pool.size());
        for (int i = 0; i < n_s; i++) {
            std::uniform_int_distribution<int> ud(i, (int)pool.size()-1);
            std::swap(pool[i], pool[ud(rng)]);
        }

        // Evaluar cada candidato con mean_dom incremental O(N)
        struct Cand { double md; int r, c; };
        std::vector<Cand> scored;
        scored.reserve(n_s);
        for (int s = 0; s < n_s; s++) {
            int r = pool[s].r, c = pool[s].c;
            long long TV_new = TV - tv[r]; // eliminar fila r
            for (int fr = 0; fr < N; fr++) {
                if (fr == r || row_used[fr]) continue;
                int diff = fr - r;
                uint64_t mask = (uint64_t)1 << c;
                if (c+diff >= 0 && c+diff < N) mask |= (uint64_t)1 << (c+diff);
                if (c-diff >= 0 && c-diff < N) mask |= (uint64_t)1 << (c-diff);
                TV_new -= __builtin_popcountll(avail[fr] & mask);
            }
            int NF_new = NF - 1;
            scored.push_back({NF_new > 0 ? (double)TV_new/NF_new : 0.0, r, c});
        }

        // Elegir entre los top_k mejores (menor mean_dom)
        std::sort(scored.begin(), scored.end(),
                  [](const Cand& a, const Cand& b){ return a.md < b.md; });
        int pick_n = std::min(top_k, (int)scored.size());
        std::uniform_int_distribution<int> ud2(0, pick_n-1);
        int chosen = ud2(rng);
        int best_r = scored[chosen].r, best_c = scored[chosen].c;

        // Colocar reina y actualizar estado incremental O(N)
        qr[k] = best_r; qc[k] = best_c;
        row_used[best_r] = true;
        for (int fr = 0; fr < N; fr++) {
            if (row_used[fr]) continue;
            int diff = fr - best_r;
            uint64_t mask = (uint64_t)1 << best_c;
            if (best_c+diff >= 0 && best_c+diff < N) mask |= (uint64_t)1 << (best_c+diff);
            if (best_c-diff >= 0 && best_c-diff < N) mask |= (uint64_t)1 << (best_c-diff);
            avail[fr] &= ~mask;
            tv[fr] = __builtin_popcountll(avail[fr]);
        }
        TV = 0; NF = 0;
        for (int fr = 0; fr < N; fr++)
            if (!row_used[fr]) { TV += tv[fr]; NF++; }
    }
    return true;
}

static void sp_collect_greedy_fast(int N, int K, long long n_att, int seed,
                                    int max_to_add, std::vector<SInst>& out,
                                    std::chrono::steady_clock::time_point t_ref, double t_cap_s,
                                    int top_k, int max_sample) {
    int K_start = kstart_for(K, N);
    if (K_start < 0) return;
    if (top_k <= 0) top_k = 5;
    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    int added = 0;
    for (long long n_gen = 0; n_gen < n_att && added < max_to_add; n_gen++) {
        if (n_gen % 200 == 0) {
            if (std::chrono::duration<double>(
                    std::chrono::steady_clock::now()-t_ref).count() > t_cap_s) break;
        }
        if (!gen_placement_greedy_fast(N, K_start, qr, qc, rng, top_k, max_sample)) continue;
        if (pipeline(qr, qc, K_start, N) != UNSAT_DET) continue;
        int cur_qr[MAXN], cur_qc[MAXN];
        memcpy(cur_qr, qr, K_start*sizeof(int));
        memcpy(cur_qc, qc, K_start*sizeof(int));
        int cur_K = K_start;
        while (cur_K > K) {
            struct Cand { double peer; int idx; };
            Cand cands[MAXN]; int nc = 0;
            for (int i = 0; i < cur_K; i++) {
                int xqr[MAXN], xqc[MAXN]; int xk = 0;
                for (int j = 0; j < cur_K; j++)
                    if (j != i) { xqr[xk]=cur_qr[j]; xqc[xk]=cur_qc[j]; xk++; }
                if (pipeline(xqr, xqc, xk, N) != UNSAT_DET) continue;
                cands[nc++] = {compute_avg_peer(xqr, xqc, xk, N), i};
            }
            if (nc == 0) break;
            if (nc > 1)
                std::sort(cands, cands+nc, [](const Cand&a, const Cand&b){
                    return a.peer < b.peer; });
            int drop = cands[0].idx;
            int nqr[MAXN], nqc[MAXN]; int nk = 0;
            for (int j = 0; j < cur_K; j++)
                if (j != drop) { nqr[nk]=cur_qr[j]; nqc[nk]=cur_qc[j]; nk++; }
            memcpy(cur_qr,nqr,nk*sizeof(int));
            memcpy(cur_qc,nqc,nk*sizeof(int));
            cur_K = nk;
        }
        if (cur_K == K) {
            SInst inst;
            memcpy(inst.qr, cur_qr, K*sizeof(int));
            memcpy(inst.qc, cur_qc, K*sizeof(int));
            out.push_back(inst); added++;
        }
    }
}

// ─── VARIANTE G: shrink agresivo sin pipeline intermedio ─────────────────────
// Genera K_start greedy → shrink agresivo guiado por mean_dom (sin pipeline por paso)
// → verifica solo el resultado final. 1 pipeline call por intento vs K_start×(K_start-K).
static void sp_collect_greedy_agressive(int N, int K, long long n_att, int seed,
                                         int max_to_add, std::vector<SInst>& out,
                                         std::chrono::steady_clock::time_point t_ref,
                                         double t_cap_s, int top_k, int K_start_override) {
    int K_start = K_start_override > 0 ? K_start_override : kstart_for(K, N);
    if (K_start < 0 || K_start >= N) return;
    if (top_k <= 0) top_k = 5;
    std::mt19937 rng(seed);
    int qr[MAXN], qc[MAXN];
    int added = 0;
    for (long long n_gen = 0; n_gen < n_att && added < max_to_add; n_gen++) {
        if (n_gen % 100 == 0) {
            if (std::chrono::duration<double>(
                    std::chrono::steady_clock::now()-t_ref).count() > t_cap_s) break;
        }
        // Generar K_start con greedy
        if (!gen_placement_greedy_mdom(N, K_start, qr, qc, rng, top_k)) continue;

        // Shrink agresivo: en cada paso quitar la reina que minimiza mean_dom del resto
        // SIN pipeline — solo guía por mean_dom
        int cur_qr[MAXN], cur_qc[MAXN];
        memcpy(cur_qr, qr, K_start*sizeof(int));
        memcpy(cur_qc, qc, K_start*sizeof(int));
        int cur_K = K_start;

        while (cur_K > K) {
            struct Cand { double md; int idx; };
            Cand best = {1e18, -1};
            for (int i = 0; i < cur_K; i++) {
                int xqr[MAXN], xqc[MAXN]; int xk = 0;
                for (int j = 0; j < cur_K; j++)
                    if (j != i) { xqr[xk]=cur_qr[j]; xqc[xk]=cur_qc[j]; xk++; }
                double md = geo_mean_dom(xqr, xqc, xk, N);
                if (md < best.md) best = {md, i};
            }
            if (best.idx < 0) break;
            int nqr[MAXN], nqc[MAXN]; int nk = 0;
            for (int j = 0; j < cur_K; j++)
                if (j != best.idx) { nqr[nk]=cur_qr[j]; nqc[nk]=cur_qc[j]; nk++; }
            memcpy(cur_qr,nqr,nk*sizeof(int));
            memcpy(cur_qc,nqc,nk*sizeof(int));
            cur_K = nk;
        }

        if (cur_K != K) continue;

        // Verificar solo al final con pipeline
        if (pipeline(cur_qr, cur_qc, K, N) != UNSAT_DET) continue;

        SInst inst;
        memcpy(inst.qr, cur_qr, K*sizeof(int));
        memcpy(inst.qc, cur_qc, K*sizeof(int));
        out.push_back(inst); added++;
    }
}

// Modo comparación: 4 variantes con mismo presupuesto de tiempo.
// Útil para calibrar qué estrategia es más eficiente por N.
void shrink_compare(int N, int K, long long n_att, double t_budget, int seed) {
    int Ks = std::min(2*K, N-1);
    double thr_c = 0.42 * N * (N - Ks) / (double)(N - K);
    printf("================================================================\n");
    printf("  SHRINK_COMPARE  N=%d  K=%d  K_start=%d  t_budget=%.0fs\n",
           N, K, Ks, t_budget);
    printf("  seed=%d  n_att=%lld (por variante)\n", seed, n_att);
    printf("  C: thr_kstart=%.3f  D: max_d=0.15*N=%.2f\n", thr_c, 0.15*N);
    printf("================================================================\n");
    fflush(stdout);

    struct Res { const char* name; int found; double secs; };
    std::vector<Res> rs;

    {
        printf("\n  [A baseline_peer]... "); fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        std::vector<SInst> out;
        sp_collect(N, K, n_att, seed, 9999, out, t0, t_budget);
        double el = std::chrono::duration<double>(
            std::chrono::steady_clock::now()-t0).count();
        rs.push_back({"A:baseline_peer", (int)out.size(), el});
        printf("found=%d  t=%.2fs\n", (int)out.size(), el); fflush(stdout);
    }
    {
        printf("  [B mdom_guidance]... "); fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        std::vector<SInst> out;
        sp_collect_mdom(N, K, n_att, seed, 9999, out, t0, t_budget);
        double el = std::chrono::duration<double>(
            std::chrono::steady_clock::now()-t0).count();
        rs.push_back({"B:mdom_guidance", (int)out.size(), el});
        printf("found=%d  t=%.2fs\n", (int)out.size(), el); fflush(stdout);
    }
    {
        printf("  [C prefilter_mdom]... "); fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        std::vector<SInst> out;
        sp_collect_prefilter(N, K, n_att, seed, 9999, out, t0, t_budget, 0.0);
        double el = std::chrono::duration<double>(
            std::chrono::steady_clock::now()-t0).count();
        rs.push_back({"C:prefilter_mdom", (int)out.size(), el});
        printf("found=%d  t=%.2fs\n", (int)out.size(), el); fflush(stdout);
    }
    {
        printf("  [D central_gen]... "); fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        std::vector<SInst> out;
        sp_collect_central(N, K, n_att, seed, 9999, out, t0, t_budget, 0.0);
        double el = std::chrono::duration<double>(
            std::chrono::steady_clock::now()-t0).count();
        rs.push_back({"D:central_gen", (int)out.size(), el});
        printf("found=%d  t=%.2fs\n", (int)out.size(), el); fflush(stdout);
    }
    {
        printf("  [E greedy_mdom top5]... "); fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        std::vector<SInst> out;
        sp_collect_greedy(N, K, n_att, seed, 9999, out, t0, t_budget, 5);
        double el = std::chrono::duration<double>(
            std::chrono::steady_clock::now()-t0).count();
        rs.push_back({"E:greedy_mdom_k5", (int)out.size(), el});
        printf("found=%d  t=%.2fs\n", (int)out.size(), el); fflush(stdout);
    }
    {
        printf("  [F greedy_fast s80]... "); fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        std::vector<SInst> out;
        sp_collect_greedy_fast(N, K, n_att, seed, 9999, out, t0, t_budget, 5, 2000);
        double el = std::chrono::duration<double>(
            std::chrono::steady_clock::now()-t0).count();
        rs.push_back({"F:greedy_fast_s80", (int)out.size(), el});
        printf("found=%d  t=%.2fs\n", (int)out.size(), el); fflush(stdout);
    }
    {
        printf("  [G greedy_agressive]... "); fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        std::vector<SInst> out;
        sp_collect_greedy_agressive(N, K, n_att, seed, 9999, out, t0, t_budget, 5, 0);
        double el = std::chrono::duration<double>(
            std::chrono::steady_clock::now()-t0).count();
        rs.push_back({"G:greedy_agressive", (int)out.size(), el});
        printf("found=%d  t=%.2fs\n", (int)out.size(), el); fflush(stdout);
    }

    printf("\n--- RESUMEN SHRINK_COMPARE N=%d K=%d ---\n", N, K);
    printf("  %-20s  %6s  %6s  %s\n", "Variante", "found", "seg", "inst/s");
    printf("  %s\n", std::string(52, '-').c_str());
    for (auto& r : rs)
        printf("  %-20s  %6d  %6.1f  %.2f\n",
               r.name, r.found, r.secs,
               r.secs > 0 ? r.found/r.secs : 0.0);
    printf("================================================================\n");
}
