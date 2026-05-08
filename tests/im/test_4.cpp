#include <cilk/cilk.h>

struct Node {
  int value;
  int weight;
};

int process(Node *n) { return n->value * n->weight; }

int compute(Node *a, Node *b) {
  int x = cilk_spawn process(a);
  int y = cilk_spawn process(b);
  cilk_sync;
  return x + y;
}

#include <stdio.h>

int main() {
  Node a;
  Node b;

  a.value = 3;
  a.weight = 4;

  b.value = 5;
  b.weight = 6;

  int result = cilk_spawn compute(&a, &b);
  cilk_sync;

  printf("result = %d\n", result);
  return 0;
}