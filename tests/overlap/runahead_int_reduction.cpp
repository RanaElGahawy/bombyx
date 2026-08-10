// Run-ahead legality: ACCEPT. The only non-induction loop-carried variable is
// `acc`, an associative integer `+` reduction; the body has no store, no spawn
// and no early exit, and the trip count is known on entry.
#include <cilk/cilk.h>
#include <cstdint>
void intRed(uint32_t u, uint64_t *pGraph, uint32_t *out) {
  uint32_t *nb = (uint32_t *)pGraph[2 * u];
  uint32_t d = pGraph[2 * u + 1];
  uint32_t acc = 0;
  #pragma BOMBYX OVERLAP
  for (uint32_t i = 0; i < d; i = i + 1) { acc += nb[i]; }
  out[u] = acc;
}
void spawner(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  for (uint32_t u = 0; u < n; u = u + 1) cilk_spawn intRed(u, pGraph, out);
  cilk_sync;
}
void top(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  cilk_spawn spawner(n, pGraph, out); cilk_sync;
}
