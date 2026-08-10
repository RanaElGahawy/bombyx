// Run-ahead legality: ACCEPT. Same loop as runahead_float_reduction.cpp, but
// REASSOC is the programmer asserting that summing the accumulator in retire
// order instead of source order is acceptable.
#include <cilk/cilk.h>
#include <cstdint>
void fpRed(uint32_t u, uint64_t *pGraph, float *out) {
  uint32_t *nb = (uint32_t *)pGraph[2 * u];
  uint32_t d = pGraph[2 * u + 1];
  float acc = 0;
  #pragma BOMBYX OVERLAP REASSOC
  for (uint32_t i = 0; i < d; i = i + 1) { acc += (float)nb[i]; }
  out[u] = acc;
}
void spawner(uint32_t n, uint64_t *pGraph, float *out) {
  for (uint32_t u = 0; u < n; u = u + 1) cilk_spawn fpRed(u, pGraph, out);
  cilk_sync;
}
void top(uint32_t n, uint64_t *pGraph, float *out) {
  cilk_spawn spawner(n, pGraph, out); cilk_sync;
}
