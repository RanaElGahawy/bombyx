// Run-ahead legality: ACCEPT. The body stores, but the store is provably
// iteration-private: `out` is fixed for the whole loop and the subscript is the
// induction variable itself, so no two iterations write the same address. The
// only other access reads `nb`, which roots at `pGraph`, not at `out`.
#include <cilk/cilk.h>
#include <cstdint>
void storing(uint32_t u, uint64_t *pGraph, uint32_t *out) {
  uint32_t *nb = (uint32_t *)pGraph[2 * u];
  uint32_t d = pGraph[2 * u + 1];
  uint32_t c = 0;
  #pragma BOMBYX OVERLAP
  for (uint32_t i = 0; i < d; i = i + 1) { out[i] = nb[i]; c += nb[i]; }
  out[u] = c;
}
void spawner(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  for (uint32_t u = 0; u < n; u = u + 1) cilk_spawn storing(u, pGraph, out);
  cilk_sync;
}
void top(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  cilk_spawn spawner(n, pGraph, out); cilk_sync;
}
