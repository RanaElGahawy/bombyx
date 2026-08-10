// Run-ahead legality: REJECT. Each iteration's store is private, but it reads
// the neighbouring slot of the same object — the one iteration i+1 writes — so
// the answer depends on which of the two runs first.
#include <cilk/cilk.h>
#include <cstdint>
void neighbour(uint32_t u, uint64_t *pGraph, uint32_t *out) {
  uint32_t d = pGraph[2 * u + 1];
  uint32_t c = 0;
  #pragma BOMBYX OVERLAP
  for (uint32_t i = 0; i < d; i = i + 1) { out[i] = out[i + 1]; c += i; }
  out[u] = c;
}
void spawner(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  for (uint32_t u = 0; u < n; u = u + 1) cilk_spawn neighbour(u, pGraph, out);
  cilk_sync;
}
void top(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  cilk_spawn spawner(n, pGraph, out); cilk_sync;
}
