// recipes.h — альтернативные рецепты одной цели (вынесено из scx.cpp). Требует elfopt.cpp до включения.
#pragma once
struct Recipe { vector<Step> st; };

static void altRecipes(Searcher& S, u64 T, int maxc, int M, vector<Recipe>& out) {
    out.clear();
    if (S.inP(T)) { out.push_back({}); return; }
    int id = S.inS1(T);
    if (id >= 0) { out.push_back({{S.f1s[id]}}); return; }      // стоимость 1: альтернативы не нужны
    if (maxc < 2) return;
    Inv iv[6], iv2[6];
    unordered_set<u64> seen;
    size_t n = S.pool.size(), off = rnd(max<size_t>(n, 1));
    // стоимость 2: T = w op p, w in S1
    for (size_t ii = 0; ii < n && (int)out.size() < M; ii++) {
        u64 x = S.pool[(ii + off) % n]; int c = inverses(T, x, S.L, iv);
        for (int q = 0; q < c && (int)out.size() < M; q++) {
            int u = S.inS1(iv[q].w); if (u < 0 || !seen.insert(iv[q].w).second) continue;
            out.push_back({{S.f1s[u], mk(iv[q], x, T)}});
        }
    }
    if (T % 2 == 0) { int u = S.inS1(T / 2); if (u >= 0 && seen.insert(T / 2).second) out.push_back({{S.f1s[u], {1, T / 2, T / 2, T}}}); }
    if (!out.empty() || maxc < 3) return;
    // стоимость 3a: T = f op g, f,g in S1
    size_t m1 = S.f1v.size(), off1 = m1 ? rnd(m1) : 0;
    set<pair<u64, u64>> seen2;
    for (size_t aa = 0; aa < m1 && (int)out.size() < M; aa++) {
        size_t a = (aa + off1) % m1; u64 x = S.f1v[a]; if (S.inP(x)) continue;
        int c = inverses(T, x, S.L, iv);
        for (int q = 0; q < c; q++) {
            u64 w = iv[q].w; if (w == x) continue;
            int u = S.inS1(w); if (u < 0) continue;
            if (!seen2.insert({min(x, w), max(x, w)}).second) continue;
            out.push_back({{S.f1s[a], S.f1s[u], mk(iv[q], x, T)}});
            if ((int)out.size() >= M) break;
        }
    }
    // стоимость 3b: T = (f op y) op x
    for (size_t ii = 0; ii < n && (int)out.size() < M; ii++) {
        u64 x = S.pool[(ii + off) % n]; int c = inverses(T, x, S.L, iv);
        for (int q = 0; q < c && (int)out.size() < M; q++) {
            u64 w1 = iv[q].w; if (S.inP(w1) || S.inS1(w1) >= 0) continue;
            for (size_t j = 0; j < n && (int)out.size() < M; j++) {
                u64 y = S.pool[j]; int c2 = inverses(w1, y, S.L, iv2);
                for (int q2 = 0; q2 < c2; q2++) {
                    int u = S.inS1(iv2[q2].w); if (u < 0) continue;
                    if (!seen2.insert({S.f1v[u], w1}).second) continue;
                    out.push_back({{S.f1s[u], mk(iv2[q2], y, w1), mk(iv[q], x, T)}});
                    break;
                }
            }
        }
    }
    if (!out.empty() || maxc < 4) return;
    // стоимость >= 4: одна (случайная) находка штатного поиска, пару попыток для разнообразия
    vector<Step> o;
    for (int r = 0; r < 2; r++) {
        int c = S.find(T, maxc, o);
        if (c < 0) break;
        out.push_back({o});
        if (c < 4) break;
    }
}
