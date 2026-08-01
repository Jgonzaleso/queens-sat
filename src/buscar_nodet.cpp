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
        if (K <= 0) {
            static const int kmin_tbl[] = {0,0,0,0,1,2,1,2,2,2,3,3,3,3,4,4,4,4,5,5};
            if (N < (int)(sizeof(kmin_tbl)/sizeof(kmin_tbl[0])))
                K = std::max(1, kmin_tbl[N]);
            else
                K = (N % 4 == 1) ? N/4 : (N+3)/4;
        }
        if (K_start <= 0) K_start = 2 * K;
        if (K_start >= N) K_start = K + std::max(2, K/2);
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
        if (K <= 0) {
            static const int kmin_tbl[] = {0,0,0,0,1,2,1,2,2,2,3,3,3,3,4,4,4,4,5,5};
            if (N < (int)(sizeof(kmin_tbl)/sizeof(kmin_tbl[0])))
                K = std::max(1, kmin_tbl[N]);
            else
                K = (N % 4 == 1) ? N/4 : (N+3)/4;
        }
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
        if (K <= 0) {
            static const int kmin_tbl[] = {0,0,0,0,1,2,1,2,2,2,3,3,3,3,4,4,4,4,5,5};
            if (N < (int)(sizeof(kmin_tbl)/sizeof(kmin_tbl[0])))
                K = std::max(1, kmin_tbl[N]);
            else
                K = (N % 4 == 1) ? N/4 : (N+3)/4;
        }
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
    if (argc > 1 && std::string(argv[1]) == "testq") {
        int N = (argc > 2) ? atoi(argv[2]) : 32;
        int K = (argc > 3) ? atoi(argv[3]) : 16;
        if (argc < 4 + 2*K) {
            printf("testq: faltan %d argumentos (r,c) de reinas\n", K);
            return 1;
        }
        int qr[MAXN], qc[MAXN];
        for (int i = 0; i < K; i++) {
            qr[i] = atoi(argv[4 + 2*i]);
            qc[i] = atoi(argv[5 + 2*i]);
        }
        g_debug_propagate = true;
        printf("DEBUG: testq N=%d K=%d queens=",N,K);
        for(int i=0;i<K;i++) printf("(%d,%d)",qr[i],qc[i]);
        printf("\n"); fflush(stdout);
        auto t0 = std::chrono::steady_clock::now();
        PResult pr = pipeline(qr, qc, K, N);
        auto t1 = std::chrono::steady_clock::now();
        float us = std::chrono::duration<float, std::micro>(t1-t0).count();
        if (pr == UNSAT_DET) {
            printf("UNSAT_DET  %.2fus\n", us);
        } else {
            memcpy(g_qr, qr, K*sizeof(int));
            memcpy(g_qc, qc, K*sizeof(int));
            long long bt = 0;
            s_ng_enabled = true; s_nogoods.clear();
            auto tg0 = std::chrono::steady_clock::now();
            greedy_rec(K, N, bt, 5000000LL);
            auto tg1 = std::chrono::steady_clock::now();
            float ms = std::chrono::duration<float, std::milli>(tg1-tg0).count();
            s_ng_enabled = false;
            printf("NO_DET  bt=%lld  %.2fms  (pipeline=%.2fus)\n", bt, ms, us);
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
