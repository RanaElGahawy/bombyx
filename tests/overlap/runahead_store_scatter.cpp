// Run-ahead legality: REJECT. The store subscript is a value loaded from
// memory, not an affine function of the induction variable, so two iterations
// can land on the same slot of `out`.
#include <cilk/cilk.h>
#include <cstdint>
void scatter(uint32_t u, uint64_t *pGraph, uint32_t *out) {
  uint32_t *nb = (uint32_t *)pGraph[2 * u];
  uint32_t d = pGraph[2 * u + 1];
  uint32_t c = 0;
  #pragma BOMBYX OVERLAP
  for (uint32_t i = 0; i < d; i = i + 1) { out[nb[i]] = i; c += nb[i]; }
  out[u] = c;
}
void spawner(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  for (uint32_t u = 0; u < n; u = u + 1) cilk_spawn scatter(u, pGraph, out);
  cilk_sync;
}
void top(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  cilk_spawn spawner(n, pGraph, out); cilk_sync;
}
