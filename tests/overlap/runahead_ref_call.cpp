// Run-ahead legality: REJECT. `step` mutates `s` through a mutable reference,
// so the loop carries the RNG state from one iteration to the next without any
// assignment appearing in the body. This is randomWalk's shape.
#include <cilk/cilk.h>
#include <cstdint>
static inline uint32_t step(uint32_t &x) { x ^= x << 13; return x; }
void walking(uint32_t u, uint64_t *pGraph, uint32_t *out) {
  uint32_t d = pGraph[2 * u + 1];
  uint32_t s = u + 1;
  uint32_t c = 0;
  // `c` is here only so the loop has a visible loop-carried variable; without
  // one OVERLAP is dropped before run-ahead legality is ever considered.
  #pragma BOMBYX OVERLAP
  for (uint32_t i = 0; i < d; i = i + 1) { out[i] = step(s); c += i; }
  out[u] = c;
}
void spawner(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  for (uint32_t u = 0; u < n; u = u + 1) cilk_spawn walking(u, pGraph, out);
  cilk_sync;
}
void top(uint32_t n, uint64_t *pGraph, uint32_t *out) {
  cilk_spawn spawner(n, pGraph, out); cilk_sync;
}
