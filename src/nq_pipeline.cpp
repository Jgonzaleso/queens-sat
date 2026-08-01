// nq_pipeline.cpp — pivot_enum, pipeline_solve, CMR analysis.
// Requiere nq_propagate.cpp incluido antes.

enum PResult { UNSAT_DET, SAT_POSSIBLE };

struct CMRResult {
    int ns;
    int min_dom;
    int avg_dom_x10;
    int detect_budget; // -1 = no detectado con budget<=2000
};

// ─── AUXILIARES ──────────────────────────────────────────────────────────────

int choose_P(const u64* d0, int ns, int max_combos, int P_max) {
    int P = 0; long long combos = 1;
    for (int i = 0; i < ns && P < P_max; i++) {
        long long nc = combos * (long long)pop64(d0[i]);
        if (nc > max_combos) break;
        combos = nc; P++;
    }
    return (P < 2) ? 2 : P;
}

static bool verify_solution(const int* cols, int N) {
    for (int r1 = 0; r1 < N; r1++) {
        if (cols[r1] < 0 || cols[r1] >= N) return false;
        for (int r2 = r1+1; r2 < N; r2++) {
            int dc = abs(cols[r1]-cols[r2]);
            if (dc == 0 || dc == r2-r1) return false;
        }
    }
    return true;
}

static bool greedy_complete(u64* dr, const int* rows, int nr, int N, int* out_cols) {
    if (nr == 0) return true;
    u64 cur[MAXN]; memcpy(cur, dr, nr*sizeof(u64));
    int rem[MAXN]; memcpy(rem, rows, nr*sizeof(int));
    int nrem = nr;
    for (int step = 0; step < nr; step++) {
        int best = 0;
        for (int i = 1; i < nrem; i++) if (pop64(cur[i]) < pop64(cur[best])) best = i;
        if (!cur[best]) return false;
        int col = ctz64(cur[best]);
        int row = rem[best];
        out_cols[row] = col;
        int nnr = 0; u64 nc[MAXN]; int nr2[MAXN]; bool ok = true;
        for (int j = 0; j < nrem && ok; j++) {
            if (j == best) continue;
            nc[nnr] = cur[j] & ~attack_mask(row, col, rem[j], N);
            if (!nc[nnr]) { ok = false; break; }
            nr2[nnr] = rem[j]; nnr++;
        }
        if (!ok) return false;
        if (nnr > 0 && propagate_all(nc, nr2, nnr, N)) return false;
        memcpy(cur, nc, nnr*sizeof(u64));
        memcpy(rem, nr2, nnr*sizeof(int));
        nrem = nnr;
    }
    return true;
}

// ─── PIVOT_ENUM: detección recursiva de UNSAT ────────────────────────────────

static PResult pivot_enum(u64* doms, const int* rows, int ns, int N,
                           int budget, int depth) {
    if (ns == 0) return SAT_POSSIBLE;
    int order[MAXN]; for (int i=0;i<ns;i++) order[i]=i;
    std::sort(order, order+ns, [&](int a,int b){ return pop64(doms[a])<pop64(doms[b]); });
    u64 sd[MAXN]; int sr[MAXN];
    for (int i=0;i<ns;i++) { sd[i]=doms[order[i]]; sr[i]=rows[order[i]]; }
    int P = choose_P(sd, ns, budget, 10); if (P > ns) P = ns;
    int pv[MAXN][MAXN], psz[MAXN];
    for (int pi=0;pi<P;pi++) {
        int sz=0; u64 tmp=sd[pi];
        while(tmp) { int c=ctz64(tmp); tmp&=tmp-1; pv[pi][sz++]=c; }
        psz[pi]=sz;
    }
    int rest[MAXN]; int nr=ns-P;
    for (int i=0;i<nr;i++) rest[i]=sr[P+i];
    if (g_mcv_enabled) {
        for (int pi=0;pi<P;pi++) {
            int imp[MAXN];
            for (int ci=0;ci<psz[pi];ci++) {
                int c=pv[pi][ci], elim=0;
                for (int i=0;i<nr;i++)
                    elim += pop64(sd[P+i] & attack_mask(sr[pi],c,rest[i],N));
                imp[ci]=elim;
            }
            for (int i=1;i<psz[pi];i++) {
                int ki=imp[i], kc=pv[pi][i], j=i-1;
                while (j>=0 && imp[j]<ki) { imp[j+1]=imp[j]; pv[pi][j+1]=pv[pi][j]; j--; }
                imp[j+1]=ki; pv[pi][j+1]=kc;
            }
        }
    }
    long long total=1; for(int p=0;p<P;p++) total*=psz[p];
    for (long long idx=0; idx<total; idx++) {
        long long tmp2=idx; int cv[MAXN];
        for (int p=P-1;p>=0;p--) { cv[p]=pv[p][tmp2%psz[p]]; tmp2/=psz[p]; }
        bool valid=true;
        for (int a=0;a<P&&valid;a++)
            for (int b=a+1;b<P&&valid;b++) {
                int di=abs(sr[a]-sr[b]);
                if (cv[a]==cv[b]||abs(cv[a]-cv[b])==di) valid=false;
            }
        if (!valid) continue;
        u64 dr[MAXN]; bool ok=true;
        for (int i=0;i<nr;i++) dr[i]=sd[P+i];
        for (int p=0;p<P&&ok;p++)
            for (int i=0;i<nr&&ok;i++) {
                dr[i] &= ~attack_mask(sr[p], cv[p], rest[i], N);
                if (!dr[i]) ok=false;
            }
        if (!ok) continue;
        if (nr==0) return SAT_POSSIBLE;
        if (propagate_all(dr, rest, nr, N)) continue;
        if (depth == 0) {
            int nsac=(nr<8)?nr:8;
            int ro[MAXN]; for (int i=0;i<nr;i++) ro[i]=i;
            std::partial_sort(ro,ro+nsac,ro+nr,[&](int a,int b){ return pop64(dr[a])<pop64(dr[b]); });
            int sr2[MAXN]; u64 sd2[MAXN];
            for (int i=0;i<nsac;i++) { sr2[i]=rest[ro[i]]; sd2[i]=dr[ro[i]]; }
            u64 d2[MAXN]; memcpy(d2, dr, nr*sizeof(u64));
            if (sac_bits(sd2,sr2,nsac,N)) continue;
            for (int i=0;i<nsac;i++) d2[ro[i]]=sd2[i];
            if (pc2_bits(d2,rest,nr,N)) continue;
            return SAT_POSSIBLE;
        } else {
            int sub_budget = std::max(4, budget/5);
            PResult r = pivot_enum(dr, rest, nr, N, sub_budget, depth-1);
            if (r == SAT_POSSIBLE) return SAT_POSSIBLE;
        }
    }
    return UNSAT_DET;
}

// ─── PIVOT_ENUM_SOLVE: detección + construcción de testigo SAT ───────────────

static PResult pivot_enum_solve(u64* doms, const int* rows, int ns, int N,
                                 int budget, int depth, int* out_cols) {
    if (ns == 0) return SAT_POSSIBLE;
    int order[MAXN]; for (int i=0;i<ns;i++) order[i]=i;
    std::sort(order, order+ns, [&](int a,int b){ return pop64(doms[a])<pop64(doms[b]); });
    u64 sd[MAXN]; int sr[MAXN];
    for (int i=0;i<ns;i++) { sd[i]=doms[order[i]]; sr[i]=rows[order[i]]; }
    int P = choose_P(sd, ns, budget, 10); if (P > ns) P = ns;
    int pv[MAXN][MAXN], psz[MAXN];
    for (int pi=0;pi<P;pi++) {
        int sz=0; u64 tmp=sd[pi];
        while(tmp) { int c=ctz64(tmp); tmp&=tmp-1; pv[pi][sz++]=c; }
        psz[pi]=sz;
    }
    int rest[MAXN]; int nr=ns-P;
    for (int i=0;i<nr;i++) rest[i]=sr[P+i];
    long long total=1; for(int p=0;p<P;p++) total*=psz[p];
    for (long long idx=0; idx<total; idx++) {
        long long tmp2=idx; int cv[MAXN];
        for (int p=P-1;p>=0;p--) { cv[p]=pv[p][tmp2%psz[p]]; tmp2/=psz[p]; }
        bool valid=true;
        for (int a=0;a<P&&valid;a++)
            for (int b=a+1;b<P&&valid;b++) {
                int di=abs(sr[a]-sr[b]);
                if (cv[a]==cv[b]||abs(cv[a]-cv[b])==di) valid=false;
            }
        if (!valid) continue;
        u64 dr[MAXN]; bool ok=true;
        for (int i=0;i<nr;i++) dr[i]=sd[P+i];
        for (int p=0;p<P&&ok;p++)
            for (int i=0;i<nr&&ok;i++) {
                dr[i] &= ~attack_mask(sr[p], cv[p], rest[i], N);
                if (!dr[i]) ok=false;
            }
        if (!ok) continue;
        if (nr==0) { for (int p=0;p<P;p++) out_cols[sr[p]]=cv[p]; return SAT_POSSIBLE; }
        if (propagate_all(dr, rest, nr, N)) continue;
        if (depth == 0) {
            int nsac=(nr<8)?nr:8;
            int ro[MAXN]; for (int i=0;i<nr;i++) ro[i]=i;
            std::partial_sort(ro,ro+nsac,ro+nr,[&](int a,int b){ return pop64(dr[a])<pop64(dr[b]); });
            int sr2[MAXN]; u64 sd2[MAXN];
            for (int i=0;i<nsac;i++) { sr2[i]=rest[ro[i]]; sd2[i]=dr[ro[i]]; }
            u64 d2[MAXN]; memcpy(d2, dr, nr*sizeof(u64));
            if (sac_bits(sd2,sr2,nsac,N)) continue;
            for (int i=0;i<nsac;i++) d2[ro[i]]=sd2[i];
            if (pc2_bits(d2,rest,nr,N)) continue;
            for (int p=0;p<P;p++) out_cols[sr[p]]=cv[p];
            if (greedy_complete(d2, rest, nr, N, out_cols)) return SAT_POSSIBLE;
            for (int p=0;p<P;p++) out_cols[sr[p]]=-1;
        } else {
            for (int p=0;p<P;p++) out_cols[sr[p]]=cv[p];
            int sub_budget = std::max(4, budget/5);
            PResult r = pivot_enum_solve(dr, rest, nr, N, sub_budget, depth-1, out_cols);
            if (r == SAT_POSSIBLE) return SAT_POSSIBLE;
            for (int p=0;p<P;p++) out_cols[sr[p]]=-1;
        }
    }
    return UNSAT_DET;
}

// ─── PIPELINE_SOLVE: retorna solución completa si SAT ────────────────────────

PResult pipeline_solve(const int* qr, const int* qc, int k, int N, int* sol_cols) {
    for (int r=0;r<N;r++) sol_cols[r]=-1;
    for (int i=0;i<k;i++) sol_cols[qr[i]]=qc[i];
    bool used[MAXN]={}; for (int i=0;i<k;i++) used[qr[i]]=true;
    int singles[MAXN]; int ns=0;
    for (int r=0;r<N;r++) {
        if (used[r]) continue;
        u64 d = available_bits(r, N, qr, qc, k);
        if (!d) return UNSAT_DET;
        singles[ns++] = r;
        sol_cols[r] = -1; // pendiente
    }
    // Computar dominios
    u64 doms0[MAXN];
    for (int i=0;i<ns;i++) doms0[i] = available_bits(singles[i], N, qr, qc, k);
    { u64 tmp[MAXN]; memcpy(tmp,doms0,ns*sizeof(u64));
      if (propagate_all(tmp,singles,ns,N)) return UNSAT_DET;
      memcpy(doms0,tmp,ns*sizeof(u64)); }
    if (ns==0) return SAT_POSSIBLE;
    if (ns==1) { sol_cols[singles[0]]=ctz64(doms0[0]); return SAT_POSSIBLE; }
    if (g_triple_enabled&&ns>=3&&ns<=32&&triple_unsat(doms0,singles,ns,N)) return UNSAT_DET;
    return pivot_enum_solve(doms0, singles, ns, N, g_max_combos, g_pipeline_depth, sol_cols);
}

// ─── PIPELINE: solo detección (sin testigo) ───────────────────────────────────

PResult pipeline(const int* qr, const int* qc, int k, int N,
                 int max_combos=-1, int P_max=10) {
    if (max_combos < 0) max_combos = g_max_combos;
    bool used[MAXN]={};
    for (int i=0;i<k;i++) used[qr[i]]=true;
    int singles[MAXN]; int ns=0;
    for (int r=0;r<N;r++) if (!used[r]) singles[ns++]=r;
    if (ns==0) return SAT_POSSIBLE;
    u64 doms0[MAXN];
    for (int i=0;i<ns;i++) {
        doms0[i]=available_bits(singles[i],N,qr,qc,k);
        if (!doms0[i]) return UNSAT_DET;
    }
    { u64 tmp[MAXN]; memcpy(tmp,doms0,ns*sizeof(u64));
      if (propagate_all(tmp,singles,ns,N)) return UNSAT_DET;
      memcpy(doms0,tmp,ns*sizeof(u64)); }
    if (ns<2) return SAT_POSSIBLE;
    if (g_triple_enabled&&ns>=3&&ns<=32&&triple_unsat(doms0,singles,ns,N)) return UNSAT_DET;
    return pivot_enum(doms0, singles, ns, N, max_combos, g_pipeline_depth);
}

// ─── CMR ANALYSIS ────────────────────────────────────────────────────────────
// Mide el mínimo budget (depth=2) con que pivot_enum detecta UNSAT.
// detect_budget=-1 → no detectado con budget≤2000.

static CMRResult cmr_analyze(const int* qr, const int* qc, int K, int N) {
    CMRResult res = {0, 0, 0, -1};
    bool used[MAXN]={}; for (int i=0;i<K;i++) used[qr[i]]=true;
    int singles[MAXN]; int ns=0;
    u64 doms0[MAXN];
    for (int r=0;r<N;r++) {
        if (used[r]) continue;
        doms0[ns]=available_bits(r,N,qr,qc,K);
        if (!doms0[ns]) { res.detect_budget=0; res.ns=ns+1; return res; }
        singles[ns]=r; ns++;
    }
    res.ns=ns;
    { u64 tmp[MAXN]; memcpy(tmp,doms0,ns*sizeof(u64));
      if (propagate_all(tmp,singles,ns,N)) { res.detect_budget=0; return res; }
      memcpy(doms0,tmp,ns*sizeof(u64)); }
    int sum_dom=0, min_dom=N+1;
    for (int i=0;i<ns;i++) {
        int sz=pop64(doms0[i]); sum_dom+=sz; min_dom=std::min(min_dom,sz);
    }
    res.min_dom=min_dom;
    res.avg_dom_x10=ns?(sum_dom*10/ns):0;
    static const int BUDGETS[]={4,8,16,25,50,100,200,400,800,2000,0};
    for (int bi=0; BUDGETS[bi]; bi++) {
        // pivot_enum no modifica doms0
        PResult r=pivot_enum(doms0, singles, ns, N, BUDGETS[bi], g_pipeline_depth);
        if (r==UNSAT_DET) { res.detect_budget=BUDGETS[bi]; break; }
    }
    return res;
}
