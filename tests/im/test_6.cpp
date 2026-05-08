#include <cilk/cilk.h>
#include <stdio.h>

struct Vec2 {
  float x;
  float y;
};

float magnitude(Vec2 *v) { return v->x * v->x + v->y * v->y; }

float compute(Vec2 *a, Vec2 *b) {
  float m1 = cilk_spawn magnitude(a);
  float m2 = cilk_spawn magnitude(b);
  cilk_sync;
  return m1 + m2;
}

int main() {
  Vec2 a = {3.0f, 4.0f};
  Vec2 b = {1.0f, 2.0f};
  float result = cilk_spawn compute(&a, &b);
  cilk_sync;
  printf("result = %f\n", result);
  return 0;
}