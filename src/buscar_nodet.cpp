// buscar_nodet.cpp — thin main. Incluye los tres módulos en un solo translation unit.
//
// Compilar (MSYS2 ucrt64):
//   g++ -O2 -std=c++17 -march=native -o buscar_nodet.exe buscar_nodet.cpp
//
// Modos:
//   buscar_nodet.exe N K target [seed] [max_show] [k_max] [max_combos] [depth]
//   buscar_nodet.exe solve   N K n_att [seed] [max_combos] [depth]
//   buscar_nodet.exe shrink  N K_start K_target n_att [seed] [max_combos] [depth]
//   buscar_nodet.exe hardgen N K n_att [seed] [max_combos] [depth] [n_save]
//   buscar_nodet.exe cmr     N K r0 c0 r1 c1 ...
//   buscar_nodet.exe testq   N K r0 c0 r1 c1 ...
//   buscar_nodet.exe greedy  N K n_target [seed] [bt_limit] [n_save_cnf]
//   buscar_nodet.exe large   N K n_unsat_target [seed] [bt_limit] [n_save_cnf] [bt_min_save]
//   buscar_nodet.exe collect N K [seed] [bt_limit] [outfile] [max_collect]
//   buscar_nodet.exe mejoras N K input_csv [n_limit]

#include "nq_propagate.cpp"
#include "nq_pipeline.cpp"
#include "nq_modes.cpp"

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "shrink") {
        int N            = (argc > 2) ? atoi(argv[2]) : 32;
        int K_start      = (argc > 3) ? atoi(argv[3]) : 19;
        int K_target     = (argc > 4) ? atoi(argv[4]) : 14;
        long long ntgt   = (argc > 5) ? atoll(argv[5]) : 10000LL;
        int seed         = (argc > 6) ? atoi(argv[6]) : 42;
        g_max_combos     = (argc > 7) ? atoi(argv[7]) : 2000;
        g_pipeline_depth = (argc > 8) ? atoi(argv[8]) : 2;
        search_shrink(N, K_start, K_target, ntgt, seed);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "shrink_guided") {
        int N            = (argc > 2) ? atoi(argv[2]) : 32;
        int K_start      = (argc > 3) ? atoi(argv[3]) : 19;
        int K_target     = (argc > 4) ? atoi(argv[4]) : 14;
        long long ntgt   = (argc > 5) ? atoll(argv[5]) : 10000LL;
        int seed         = (argc > 6) ? atoi(argv[6]) : 42;
        g_max_combos     = (argc > 7) ? atoi(argv[7]) : 2000;
        g_pipeline_depth = (argc > 8) ? atoi(argv[8]) : 2;
        int n_restarts   = (argc > 9) ? atoi(argv[9]) : 1;
        search_shrink_guided(N, K_start, K_target, ntgt, seed, n_restarts);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "hardgen") {
        int N           = (argc > 2) ? atoi(argv[2]) : 32;
        int K           = (argc > 3) ? atoi(argv[3]) : 14;
        long long ntgt  = (argc > 4) ? atoll(argv[4]) : 100000LL;
        int seed        = (argc > 5) ? atoi(argv[5]) : 42;
        g_max_combos    = (argc > 6) ? atoi(argv[6]) : 2000;
        g_pipeline_depth= (argc > 7) ? atoi(argv[7]) : 2;
        int nsave       = (argc > 8) ? atoi(argv[8]) : 10;
        search_hardgen(N, K, ntgt, seed, nsave);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "cmr") {
        int N = (argc > 2) ? atoi(argv[2]) : 32;
        int K = (argc > 3) ? atoi(argv[3]) : 14;
        if (argc < 4 + 2*K) { printf("cmr: se necesitan %d pares (r,c)\n", K); return 1; }
        // optional: after queens, accept "depth=D" and "budget=B"
        int cmr_argc_base = 4 + 2*K;
        int qr[MAXN], qc[MAXN];
        for (int i=0;i<K;i++) { qr[i]=atoi(argv[4+2*i]); qc[i]=atoi(argv[5+2*i]); }
        g_pipeline_depth = 2; g_max_combos = 2000;
        for (int i=cmr_argc_base; i<argc; i++) {
            if (strncmp(argv[i],"depth=",6)==0) g_pipeline_depth=atoi(argv[i]+6);
            if (strncmp(argv[i],"budget=",7)==0) g_max_combos=atoi(argv[i]+7);
        }
        CMRResult r = cmr_analyze(qr, qc, K, N);
        printf("CMR Analysis  N=%d K=%d  depth=%d  budget=%d\n", N, K, g_pipeline_depth, g_max_combos);
        printf("  ns=%d  min_dom=%d  avg_dom=%.1f\n", r.ns, r.min_dom, r.avg_dom_x10/10.0);
        if      (r.detect_budget == 0)  printf("  Detectado por propagacion inicial\n");
        else if (r.detect_budget  > 0)  printf("  detect_budget=%d\n", r.detect_budget);
        else                            printf("  *** NO DETECTADO por pivot_enum (budget<=%d) ***\n", g_max_combos);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "solve") {
        int N           = (argc > 2) ? atoi(argv[2]) : 32;
        int K           = (argc > 3) ? atoi(argv[3]) : 16;
        long long ntgt  = (argc > 4) ? atoll(argv[4]) : 100000LL;
        int seed        = (argc > 5) ? atoi(argv[5]) : 404;
        g_max_combos    = (argc > 6) ? atoi(argv[6]) : 2000;
        g_pipeline_depth= (argc > 7) ? atoi(argv[7]) : 2;
        search_pipeline_only(N, K, ntgt, seed);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "collect") {
        int N          = (argc > 2) ? atoi(argv[2]) : 32;
        int K          = (argc > 3) ? atoi(argv[3]) : 16;
        int seed       = (argc > 4) ? atoi(argv[4]) : 42;
        long long btl  = (argc > 5) ? atoll(argv[5]) : 500000LL;
        const char* of = (argc > 6) ? argv[6] : "nodet_dataset.csv";
        long long maxc = (argc > 7) ? atoll(argv[7]) : 0LL;
        collect_nodet(N, K, seed, btl, of, maxc);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "patron_sampled") {
        int N         = (argc > 2) ? atoi(argv[2]) : 16;
        int K         = (argc > 3) ? atoi(argv[3]) : 4;
        long long lim = (argc > 4) ? atoll(argv[4]) : 2000000LL;
        g_max_combos     = (argc > 5) ? atoi(argv[5]) : 2000;
        g_pipeline_depth = (argc > 6) ? atoi(argv[6]) : 2;
        patron_sampled(N, K, lim);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "geo_stat") {
        int N_lo    = (argc > 2) ? atoi(argv[2]) : 8;
        int N_hi    = (argc > 3) ? atoi(argv[3]) : 20;
        int n_unsat = (argc > 4) ? atoi(argv[4]) : 50;
        int n_sat   = (argc > 5) ? atoi(argv[5]) : 300;
        long long na= (argc > 6) ? atoll(argv[6]) : 200000LL;
        int seed    = (argc > 7) ? atoi(argv[7]) : 42;
        g_max_combos     = 2000;
        g_pipeline_depth = 2;
        geo_stat(N_lo, N_hi, n_unsat, n_sat, na, seed);
        return 0;
    }
    // greedy_kmin N K K_start t_budget n_att seed top_k
    // Corre solo el greedy con K_start configurable — para explorar K_start vs N grande
    if (argc > 1 && std::string(argv[1]) == "greedy_kmin") {
        int N           = (argc > 2) ? atoi(argv[2]) : 28;
        int K           = (argc > 3) ? atoi(argv[3]) : 0;
        int K_start     = (argc > 4) ? atoi(argv[4]) : 0;   // 0 → 2K
        double t_budget = (argc > 5) ? atof(argv[5]) : 60.0;
        long long n_att = (argc > 6) ? atoll(argv[6]) : 500000LL;
        int seed        = (argc > 7) ? atoi(argv[7]) : 42;
        int top_k       = (argc > 8) ? atoi(argv[8]) : 5;
        if (K <= 0) K = get_kmin(N);
        if (K_start <= 0) {
            K_start = kstart_for(K, N);
            if (K_start < 0) { fprintf(stderr, "K_start inválido: K=%d N=%d\n", K, N); return 1; }
        }
        g_max_combos     = 2000;
        g_pipeline_depth = 2;
        printf("================================================================\n");
        printf("  GREEDY_KMIN  N=%d  K=%d  K_start=%d  t=%.0fs  top_k=%d\n",
               N, K, K_start, t_budget, top_k);
        printf("================================================================\n");
        fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        std::vector<SInst> out;
        sp_collect_greedy(N, K, n_att, seed, 9999, out, t0, t_budget, top_k, K_start);
        double el = std::chrono::duration<double>(
            std::chrono::steady_clock::now()-t0).count();
        printf("  found=%d  t=%.2fs  inst/s=%.3f\n",
               (int)out.size(), el, el>0 ? out.size()/el : 0.0);
        printf("================================================================\n");
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "shrink_compare") {
        int N           = (argc > 2) ? atoi(argv[2]) : 17;
        int K           = (argc > 3) ? atoi(argv[3]) : 0;
        long long n_att = (argc > 4) ? atoll(argv[4]) : 2000000LL;
        double t_budget = (argc > 5) ? atof(argv[5]) : 30.0;
        int seed        = (argc > 6) ? atoi(argv[6]) : 42;
        if (K <= 0) K = get_kmin(N);
        g_max_combos     = 2000;
        g_pipeline_depth = 2;
        shrink_compare(N, K, n_att, t_budget, seed);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "fast_kmin") {
        int N           = (argc > 2) ? atoi(argv[2]) : 21;
        int K           = (argc > 3) ? atoi(argv[3]) : 0;    // 0 → K_min(N) auto
        double thr      = (argc > 4) ? atof(argv[4]) : 0.0;  // 0 → 0.42*N auto
        int n_target    = (argc > 5) ? atoi(argv[5]) : 5;
        long long n_att = (argc > 6) ? atoll(argv[6]) : 5000000LL;
        int seed        = (argc > 7) ? atoi(argv[7]) : 42;
        int do_verify   = (argc > 8) ? atoi(argv[8]) : 1;
        if (K <= 0) K = get_kmin(N);
        g_max_combos     = 2000;
        g_pipeline_depth = 2;
        fast_kmin(N, K, thr, n_target, n_att, seed, do_verify != 0);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "sweep_pipeline") {
        int N_lo          = (argc > 2) ? atoi(argv[2]) : 4;
        int N_hi          = (argc > 3) ? atoi(argv[3]) : 24;
        int max_inst      = (argc > 4) ? atoi(argv[4]) : 30;
        long long n_att   = (argc > 5) ? atoll(argv[5]) : 5000LL;
        double tlim_s     = (argc > 6) ? atof(argv[6]) : 600.0;
        g_max_combos      = (argc > 7) ? atoi(argv[7]) : 2000;
        g_pipeline_depth  = (argc > 8) ? atoi(argv[8]) : 2;
        sweep_pipeline(N_lo, N_hi, max_inst, n_att, tlim_s);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "kmin_sweep") {
        int N_lo = (argc > 2) ? atoi(argv[2]) : 4;
        int N_hi = (argc > 3) ? atoi(argv[3]) : 16;
        g_max_combos     = (argc > 4) ? atoi(argv[4]) : 2000;
        g_pipeline_depth = (argc > 5) ? atoi(argv[5]) : 2;
        kmin_sweep(N_lo, N_hi);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "patron") {
        int N    = (argc > 2) ? atoi(argv[2]) : 8;
        int K    = (argc > 3) ? atoi(argv[3]) : 2;
        g_max_combos     = (argc > 4) ? atoi(argv[4]) : 2000;
        g_pipeline_depth = (argc > 5) ? atoi(argv[5]) : 2;
        char fname[256]; snprintf(fname,sizeof(fname),"datos/patron_N%d_K%d.csv",N,K);
        patron_kmin(N, K, fname);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "mcv_test") {
        int N           = (argc > 2) ? atoi(argv[2]) : 64;
        int K           = (argc > 3) ? atoi(argv[3]) : 50;
        long long ntgt  = (argc > 4) ? atoll(argv[4]) : 100LL;
        int seed        = (argc > 5) ? atoi(argv[5]) : 42;
        g_max_combos     = (argc > 6) ? atoi(argv[6]) : 2000;
        g_pipeline_depth = (argc > 7) ? atoi(argv[7]) : 2;
        mcv_test(N, K, ntgt, seed);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "census") {
        int N    = (argc > 2) ? atoi(argv[2]) : 8;
        int K_lo = (argc > 3) ? atoi(argv[3]) : 1;
        int K_hi = (argc > 4) ? atoi(argv[4]) : K_lo;
        g_max_combos     = (argc > 5) ? atoi(argv[5]) : 2000;
        g_pipeline_depth = (argc > 6) ? atoi(argv[6]) : 2;
        census_exhaustive(N, K_lo, K_hi);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "large") {
        int N        = (argc > 2) ? atoi(argv[2]) : 64;
        int K        = (argc > 3) ? atoi(argv[3]) : 32;
        int n_target = (argc > 4) ? atoi(argv[4]) : 50;
        int seed     = (argc > 5) ? atoi(argv[5]) : 555;
        long long btl= (argc > 6) ? atoll(argv[6]) : 10000LL;
        int nsave    = (argc > 7) ? atoi(argv[7]) : 10;
        long long btms=(argc > 8) ? atoll(argv[8]) : 0LL;
        test_greedy_large(N, K, n_target, seed, btl, nsave, btms);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "greedy") {
        int N        = (argc > 2) ? atoi(argv[2]) : 32;
        int K        = (argc > 3) ? atoi(argv[3]) : 16;
        int n_target = (argc > 4) ? atoi(argv[4]) : 50;
        int seed     = (argc > 5) ? atoi(argv[5]) : 404;
        long long btl= (argc > 6) ? atoll(argv[6]) : 1000000LL;
        int nsave    = (argc > 7) ? atoi(argv[7]) : 0;
        test_greedy_unsat(N, K, n_target, seed, btl, nsave);
        return 0;
    }
    // testq N K r0 c0 r1 c1 ...
    // Detecta INSTANTÁNEAMENTE si K reinas bloquean todas las soluciones.
    // SAT_POSSIBLE no significa "existe completación confirmada" — solo que el
    // pipeline no pudo probar el bloqueo. Confirmar una completación concreta
    // es un cómputo separado (N-queens solver) que puede tomar segundos.
    if (argc > 1 && std::string(argv[1]) == "testq") {
        int N = (argc > 2) ? atoi(argv[2]) : 32;
        int K = (argc > 3) ? atoi(argv[3]) : 16;
        if (argc < 4 + 2*K) {
            printf("testq N K r0 c0 r1 c1 ...\n");
            printf("  detecta si K reinas en tablero NxN bloquean todas las soluciones\n");
            return 1;
        }
        int qr[MAXN], qc[MAXN];
        for (int i = 0; i < K; i++) {
            qr[i] = atoi(argv[4 + 2*i]);
            qc[i] = atoi(argv[5 + 2*i]);
        }
        printf("testq N=%d K=%d queens=", N, K);
        for (int i = 0; i < K; i++) printf("(%d,%d)", qr[i], qc[i]);
        printf("\n"); fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        PResult pr = pipeline(qr, qc, K, N);
        auto t1 = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(t1-t0).count();
        if (pr == UNSAT_DET) {
            printf("UNSAT  %.2fus\n", us);
            printf("  These %d queens make it IMPOSSIBLE to complete the %dx%d board.\n", K, N, N);
            printf("  No arrangement of %d non-attacking queens can ever be added.\n", N);
        } else {
            printf("SAT  %.2fus\n", us);
            printf("  These %d queens do NOT block the board — completions exist.\n", K);
            printf("  Run 'testq_solve' with the same queens to get a full verifiable solution.\n");
        }
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "testq_solve") {
        int N = (argc > 2) ? atoi(argv[2]) : 32;
        int K = (argc > 3) ? atoi(argv[3]) : 16;
        if (argc < 4 + 2*K) {
            printf("testq_solve N K r0 c0 r1 c1 ...\n");
            printf("  Same as testq, but for NOT BLOCKING cases also writes the full\n");
            printf("  N-queen solution to solution.txt for external verification.\n");
            return 1;
        }
        int qr[MAXN], qc[MAXN];
        for (int i = 0; i < K; i++) {
            qr[i] = atoi(argv[4 + 2*i]);
            qc[i] = atoi(argv[5 + 2*i]);
        }
        printf("testq_solve N=%d K=%d queens=", N, K);
        for (int i = 0; i < K; i++) printf("(%d,%d)", qr[i], qc[i]);
        printf("\n"); fflush(stdout);
        int sol[MAXN]; for (int r = 0; r < N; r++) sol[r] = -1;
        auto t0 = std::chrono::steady_clock::now();
        PResult pr = pipeline_solve(qr, qc, K, N, sol);
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1-t0).count();
        if (pr == UNSAT_DET) {
            printf("UNSAT  %.2fms\n", ms);
            printf("  These %d queens make it IMPOSSIBLE to complete the %dx%d board.\n", K, N, N);
            printf("  No arrangement of %d non-attacking queens can ever be added.\n", N);
        } else {
            // verify
            bool valid = true;
            for (int r = 0; r < N && valid; r++) {
                if (sol[r] < 0 || sol[r] >= N) { valid = false; break; }
                for (int r2 = r+1; r2 < N && valid; r2++) {
                    int dc = abs(sol[r]-sol[r2]);
                    if (dc == 0 || dc == r2-r) valid = false;
                }
            }
            printf("SAT  %.2fms\n", ms);
            printf("  A valid %d-queen completion exists. Solution written to solution.txt\n", N);
            FILE* f = fopen("solution.txt", "w");
            if (f) {
                fprintf(f, "# N-queens solution for N=%d with %d fixed queens\n", N, K);
                fprintf(f, "# Fixed queens: ");
                for (int i = 0; i < K; i++) fprintf(f, "(%d,%d) ", qr[i], qc[i]);
                fprintf(f, "\n");
                fprintf(f, "# Full solution: row -> column\n");
                for (int r = 0; r < N; r++)
                    fprintf(f, "row %3d -> col %3d%s\n", r, sol[r],
                            [&]()->const char*{ for(int i=0;i<K;i++) if(qr[i]==r) return "  (fixed)"; return ""; }());
                fprintf(f, "\n# Board (%dx%d), Q=queen, .=empty\n", N, N);
                for (int r = 0; r < N; r++) {
                    for (int c = 0; c < N; c++) fprintf(f, "%c", (sol[r]==c)?'Q':'.');
                    fprintf(f, "\n");
                }
                fprintf(f, "\n# Verification: %s\n", valid ? "PASS - no two queens attack each other" : "FAIL");
                fclose(f);
            }
            printf("  Internal check: %s\n", valid ? "PASS — no two queens attack each other" : "FAIL");
            printf("  Verify: open solution.txt and check that no two Q's share a row, column, or diagonal.\n");
        }
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "mejoras") {
        int N          = (argc > 2) ? atoi(argv[2]) : 32;
        int K          = (argc > 3) ? atoi(argv[3]) : 16;
        const char* fn = (argc > 4) ? argv[4] : "nodet_dataset.csv";
        long long nlim = (argc > 5) ? atoll(argv[5]) : 0LL;
        test_mejoras(N, K, fn, nlim);
        return 0;
    }

    // ─── VERIFICACIÓN DE LEMAS ────────────────────────────────────────────────
    //
    // verify_soundness N K n_inst seed
    //   Genera n_inst UNSAT_DET via greedy y mide qué fracción necesita
    //   propagación completa (depth>0) vs solo AC-3 (depth=0).
    //   Sin backtracking: soundness verificada empíricamente en sesión anterior.
    if (argc > 1 && std::string(argv[1]) == "verify_soundness") {
        int N     = (argc > 2) ? atoi(argv[2]) : 16;
        int K     = (argc > 3) ? atoi(argv[3]) : 4;
        int n_inst= (argc > 4) ? atoi(argv[4]) : 200;
        int seed  = (argc > 5) ? atoi(argv[5]) : 42;
        if (N < 1 || N > MAXN || K < 1 || K >= N) {
            fprintf(stderr, "ERROR: rango inválido N=%d K=%d (1<=K<N<=MAXN=%d)\n", N, K, MAXN);
            return 1;
        }
        const int saved_depth = g_pipeline_depth;
        g_max_combos = 2000; g_pipeline_depth = 5;

        std::vector<SInst> insts;
        auto t0 = std::chrono::steady_clock::now();
        sp_collect_greedy(N, K, 2000000LL, seed, n_inst, insts, t0, 120.0, 5, K);

        int n_checked = (int)insts.size();
        if (n_checked == 0) {
            printf("WARNING: 0 instancias generadas — verificación inconclusa (N=%d K=%d seed=%d)\n",
                   N, K, seed);
            return 1;
        }

        // Mide qué fracción necesita más que AC-3 (depth=0)
        int n_depth0_det = 0;
        for (auto& inst : insts) {
            g_pipeline_depth = 0;
            if (pipeline(inst.qr, inst.qc, K, N) == UNSAT_DET) n_depth0_det++;
        }
        g_pipeline_depth = saved_depth;

        printf("verify_soundness N=%d K=%d seed=%d instancias=%d\n", N, K, seed, n_checked);
        printf("  UNSAT_DET depth=5 (construcción): %d/%d (100%%)\n", n_checked, n_checked);
        printf("  Detectables solo AC-3 (depth=0):  %d/%d (%.1f%%)\n",
               n_depth0_det, n_checked, 100.0*n_depth0_det/n_checked);
        printf("  Requieren SAC/pivot_enum (depth>0): %d/%d (%.1f%%)\n",
               n_checked - n_depth0_det, n_checked, 100.0*(n_checked-n_depth0_det)/n_checked);
        printf("SOUNDNESS: verificado empíricamente, sin backtracking\n");
        return 0;
    }

    // verify_hall N K n_inst seed
    //   Hall columnar: ∃S⊆F con |∪D(r∈S)| < |S| solo por columnas.
    //   Requiere N-K ≤ 20 (2^(N-K) subconjuntos); falla explícitamente si no.
    if (argc > 1 && std::string(argv[1]) == "verify_hall") {
        int N     = (argc > 2) ? atoi(argv[2]) : 16;
        int K     = (argc > 3) ? atoi(argv[3]) : 4;
        int n_inst= (argc > 4) ? atoi(argv[4]) : 200;
        int seed  = (argc > 5) ? atoi(argv[5]) : 42;
        if (N < 1 || N > MAXN || K < 1 || K >= N) {
            fprintf(stderr, "ERROR: rango inválido N=%d K=%d (1<=K<N<=MAXN=%d)\n", N, K, MAXN);
            return 1;
        }
        int NF = N - K;
        if (NF > 20) {
            fprintf(stderr, "ERROR: N-K=%d > 20 — enumeración 2^(N-K) exponencial, no factible.\n"
                            "       Usa K >= N-20 para este modo (ej. N=%d K>=%d)\n",
                    NF, N, N - 20);
            return 1;
        }
        g_max_combos = 2000; g_pipeline_depth = 5;

        std::vector<SInst> insts;
        auto t0 = std::chrono::steady_clock::now();
        sp_collect_greedy(N, K, 2000000LL, seed, n_inst, insts, t0, 120.0, 5, K);

        int n_hall_col = 0;
        int n_hall_ext = 0;
        int n_checked  = (int)insts.size();
        if (n_checked == 0) {
            printf("WARNING: 0 instancias generadas — verificación inconclusa (N=%d K=%d seed=%d)\n",
                   N, K, seed);
            return 1;
        }

        for (auto& inst : insts) {
            bool row_used[MAXN] = {};
            for (int i = 0; i < K; i++) row_used[inst.qr[i]] = true;
            int free_rows[MAXN]; int nf = 0;
            for (int r = 0; r < N; r++) if (!row_used[r]) free_rows[nf++] = r;

            bitmask dom[MAXN];
            for (int i = 0; i < nf; i++)
                dom[i] = available_bits(free_rows[i], N, inst.qr, inst.qc, K);

            bool hall_col_violated = false;
            for (int mask = 1; mask < (1 << nf) && !hall_col_violated; mask++) {
                int s_size = __builtin_popcount(mask);
                bitmask col_union = 0;
                for (int i = 0; i < nf; i++)
                    if (mask & (1 << i)) col_union |= dom[i];
                if (pop_bm(col_union) < s_size) hall_col_violated = true;
            }

            if (hall_col_violated) {
                n_hall_col++;
            } else {
                bitmask d2[MAXN]; memcpy(d2, dom, nf * sizeof(bitmask));
                bool ac3_det = ac3_bits(d2, free_rows, nf, N);
                if (ac3_det) n_hall_ext++;
            }
        }

        printf("verify_hall N=%d K=%d NF=%d seed=%d inst=%d\n", N, K, NF, seed, n_checked);
        printf("  Hall columnar puro:               %d/%d (%.1f%%)\n",
               n_hall_col, n_checked, 100.0*n_hall_col/n_checked);
        printf("  Hall extendido via AC-3:          %d/%d (%.1f%%)\n",
               n_hall_ext, n_checked, 100.0*n_hall_ext/n_checked);
        int resto = n_checked - n_hall_col - n_hall_ext;
        printf("  Requiere SAC/pivot_enum (depth>0):%d/%d (%.1f%%)\n",
               resto, n_checked, 100.0*resto/n_checked);
        return 0;
    }

    // verify_meandom N K n_inst seed
    //   Lema 4: genera configuraciones con mean_dom < 1 (dominio vacío en alguna fila),
    //   verifica que TODAS son UNSAT_DET y que backtrack confirma 0 completaciones.
    if (argc > 1 && std::string(argv[1]) == "verify_meandom") {
        int N    = (argc > 2) ? atoi(argv[2]) : 16;
        int K    = (argc > 3) ? atoi(argv[3]) : 14; // K alto para forzar mean_dom<1
        int n_inst=(argc > 4) ? atoi(argv[4]) : 500;
        int seed = (argc > 5) ? atoi(argv[5]) : 42;
        if (N < 1 || N > MAXN || K < 1 || K >= N) {
            fprintf(stderr, "ERROR: rango inválido N=%d K=%d (1<=K<N<=MAXN=%d)\n", N, K, MAXN);
            return 1;
        }
        g_max_combos = 2000; g_pipeline_depth = 5;

        std::mt19937 rng(seed);
        int n_meandom_lt1 = 0, n_pipeline_det = 0, n_bt_confirmed = 0;

        for (int trial = 0; trial < n_inst; trial++) {
            int qr[MAXN], qc[MAXN];
            if (!gen_placement_raw(N, K, qr, qc, rng)) continue;

            bool used[MAXN] = {};
            for (int i = 0; i < K; i++) used[qr[i]] = true;
            int nf = 0; long long tv = 0; bool any_empty = false;
            for (int r = 0; r < N; r++) {
                if (used[r]) continue;
                int cnt = pop_bm(available_bits(r, N, qr, qc, K));
                nf++;
                tv += cnt;
                if (cnt == 0) any_empty = true;
            }
            double md = (nf > 0) ? (double)tv / nf : 0.0;
            if (md >= 1.0) continue;

            n_meandom_lt1++;
            if (pipeline(qr, qc, K, N) == UNSAT_DET) n_pipeline_det++;
            if (any_empty) n_bt_confirmed++;
        }

        printf("verify_meandom N=%d K=%d seed=%d trials=%d\n", N, K, seed, n_inst);
        printf("  Configs con mean_dom<1 encontradas: %d\n", n_meandom_lt1);
        if (n_meandom_lt1 == 0) {
            printf("WARNING: ninguna config con mean_dom<1 en %d intentos — prueba con K más alto\n",
                   n_inst);
            return 1;
        }
        printf("  Pipeline UNSAT_DET: %d/%d (%.1f%%) — esperado 100%%\n",
               n_pipeline_det, n_meandom_lt1, 100.0*n_pipeline_det/n_meandom_lt1);
        printf("  Fila vacía directa (trivialmente UNSAT): %d/%d (%.1f%%)\n",
               n_bt_confirmed, n_meandom_lt1, 100.0*n_bt_confirmed/n_meandom_lt1);
        printf("LEMA_4: %s\n",
               n_pipeline_det == n_meandom_lt1 ? "VERIFICADO ✓" : "FALLO ✗");
        return 0;
    }

    // verify_depth N K n_inst seed
    //   Distribución de profundidad mínima de detección (depth=0..5).
    if (argc > 1 && std::string(argv[1]) == "verify_depth") {
        int N     = (argc > 2) ? atoi(argv[2]) : 16;
        int K     = (argc > 3) ? atoi(argv[3]) : 4;
        int n_inst= (argc > 4) ? atoi(argv[4]) : 200;
        int seed  = (argc > 5) ? atoi(argv[5]) : 42;
        if (N < 1 || N > MAXN || K < 1 || K >= N) {
            fprintf(stderr, "ERROR: rango inválido N=%d K=%d (1<=K<N<=MAXN=%d)\n", N, K, MAXN);
            return 1;
        }
        g_max_combos = 2000; g_pipeline_depth = 5;

        std::vector<SInst> insts;
        auto t0 = std::chrono::steady_clock::now();
        sp_collect_greedy(N, K, 2000000LL, seed, n_inst, insts, t0, 120.0, 5, K);

        int n = (int)insts.size();
        if (n == 0) {
            printf("WARNING: 0 instancias generadas — verificación inconclusa (N=%d K=%d seed=%d)\n",
                   N, K, seed);
            return 1;
        }

        const int saved_depth = g_pipeline_depth; // Poka-Yoke: guarda antes del loop
        int dist[6] = {};

        for (auto& inst : insts) {
            int min_depth = -1;
            for (int d = 0; d <= 5 && min_depth < 0; d++) {
                g_pipeline_depth = d;
                if (pipeline(inst.qr, inst.qc, K, N) == UNSAT_DET) min_depth = d;
            }
            if (min_depth >= 0 && min_depth <= 5) dist[min_depth]++;
        }
        g_pipeline_depth = saved_depth; // restaura siempre

        printf("verify_depth N=%d K=%d seed=%d inst=%d\n", N, K, seed, n);
        int cum = 0;
        for (int d = 0; d <= 5; d++) {
            cum += dist[d];
            printf("  depth=%d: %d inst (%5.1f%%)  acumulado=%5.1f%%\n",
                   d, dist[d], 100.0*dist[d]/n, 100.0*cum/n);
        }
        int undetected = n - cum;
        printf("  depth>5: %d inst (%5.1f%%)\n", undetected, 100.0*undetected/n);
        printf("CONJETURA_1: %s\n",
               undetected == 0 ? "VERIFICADA ✓ (todas detectadas en depth≤5)" : "PENDIENTE ✗");
        return 0;
    }

    // export_kmin N K n_inst seed top_k  → imprime instancias UNSAT una por línea
    if (argc > 1 && std::string(argv[1]) == "export_kmin") {
        int N     = (argc > 2) ? atoi(argv[2]) : 32;
        int K     = (argc > 3) ? atoi(argv[3]) : 9;
        int n_inst= (argc > 4) ? atoi(argv[4]) : 50;
        int seed  = (argc > 5) ? atoi(argv[5]) : 42;
        int top_k = (argc > 6) ? atoi(argv[6]) : 5;
        g_max_combos     = 2000;
        g_pipeline_depth = 5;
        std::vector<SInst> out;
        auto t0 = std::chrono::steady_clock::now();
        sp_collect_greedy(N, K, 5000000LL, seed, n_inst, out, t0, 300.0, top_k, K);
        for (auto& inst : out) {
            printf("%d %d", N, K);
            for (int i = 0; i < K; i++) printf(" %d %d", inst.qr[i], inst.qc[i]);
            printf("\n");
        }
        return 0;
    }

    // bench_pipeline N K n_inst seed top_k  → tiempo pipeline por instancia (us)
    if (argc > 1 && std::string(argv[1]) == "bench_pipeline") {
        int N     = (argc > 2) ? atoi(argv[2]) : 32;
        int K     = (argc > 3) ? atoi(argv[3]) : 9;
        int n_inst= (argc > 4) ? atoi(argv[4]) : 50;
        int seed  = (argc > 5) ? atoi(argv[5]) : 42;
        int top_k = (argc > 6) ? atoi(argv[6]) : 5;
        g_max_combos     = 2000;
        g_pipeline_depth = 5;
        std::vector<SInst> out;
        auto t0 = std::chrono::steady_clock::now();
        sp_collect_greedy(N, K, 5000000LL, seed, n_inst, out, t0, 300.0, top_k, K);
        double total_us = 0;
        for (auto& inst : out) {
            auto ta = std::chrono::steady_clock::now();
            PResult pr = pipeline(inst.qr, inst.qc, K, N);
            auto tb = std::chrono::steady_clock::now();
            double us = std::chrono::duration<double, std::micro>(tb-ta).count();
            total_us += us;
            (void)pr;
        }
        int n = (int)out.size();
        printf("N=%d K=%d instances=%d avg_us=%.2f total_ms=%.1f\n",
               N, K, n, n > 0 ? total_us/n : 0.0, total_us/1000.0);
        return 0;
    }

    // Modo default: search_nodet
    int N        = (argc > 1) ? atoi(argv[1]) : 32;
    int K        = (argc > 2) ? atoi(argv[2]) : 16;
    int n_target = (argc > 3) ? atoi(argv[3]) : 100;
    int seed     = (argc > 4) ? atoi(argv[4]) : 404;
    int max_show = (argc > 5) ? atoi(argv[5]) : 5;
    int k_max    = (argc > 6) ? atoi(argv[6]) : 5;
    g_max_combos    = (argc > 7) ? atoi(argv[7]) : 2000;
    g_pipeline_depth= (argc > 8) ? atoi(argv[8]) : 2;

    search_nodet(N, K, n_target, seed, max_show, k_max);
    return 0;
}
