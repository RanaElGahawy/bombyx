#include "tbb_explicit.hh"
#include <stdio.h>

int main();

class fun_task : public tbb::task {
public:
  long n;
  void *bx_out;
  explicit fun_task(void *out) : bx_out(out) {}
  tbb::task *execute() override;
};
class fun_afterif0_task : public tbb::task {
public:
  long n;
  long w;
  long x;
  long y;
  long f;
  long h;
  void *bx_out;
  explicit fun_afterif0_task(void *out) : bx_out(out) {}
  tbb::task *execute() override;
};
class fun_cont0_task : public tbb::task {
public:
  long n;
  long x;
  long y;
  void *bx_out;
  explicit fun_cont0_task(void *out) : bx_out(out) {}
  tbb::task *execute() override;
};
class fun_cont1_task : public tbb::task {
public:
  long n;
  long x;
  long y;
  long f;
  long h;
  void *bx_out;
  explicit fun_cont1_task(void *out) : bx_out(out) {}
  tbb::task *execute() override;
};
class main_cont0_task : public tbb::task {
public:
  int n;
  void *bx_out;
  explicit main_cont0_task(void *out) : bx_out(out) {}
  tbb::task *execute() override;
};

tbb::task *fun_task::execute() {
  long w;
  long x;
  long y;
  long f;
  long h;
  w = 14;
  if ((n > 2)) {
    fun_cont0_task &SN_fun_cont0 =
        *new (allocate_continuation()) fun_cont0_task(bx_out);
    tbb::task_list SN_fun_cont0_list;
    int SN_fun_cont0_cnt = 0;
    fun_task &sp0 =
        *new (SN_fun_cont0.allocate_child()) fun_task(&SN_fun_cont0.x);
    sp0.n = (n - 5);
    SN_fun_cont0_list.push_back(sp0);
    ++SN_fun_cont0_cnt;

    fun_task &sp1 =
        *new (SN_fun_cont0.allocate_child()) fun_task(&SN_fun_cont0.y);
    sp1.n = (n - 2);
    SN_fun_cont0_list.push_back(sp1);
    ++SN_fun_cont0_cnt;

    SN_fun_cont0.n = n;
    // Original sync was here
    if (SN_fun_cont0_cnt > 0) {
      SN_fun_cont0.set_ref_count(SN_fun_cont0_cnt);
      tbb::task::spawn(SN_fun_cont0_list);
    } else {
      tbb::task::spawn(SN_fun_cont0);
    }
  } else {
    fun_afterif0_task &sp2 =
        *new (allocate_continuation()) fun_afterif0_task(bx_out);
    sp2.n = n;
    sp2.w = w;
    sp2.x = x;
    sp2.y = y;
    sp2.f = f;
    sp2.h = h;
    tbb::task::spawn(sp2);
    return nullptr;
  }
  return nullptr;
}
int main() {
  int n;
  tbb::task_scheduler_init bx_tbb_init;
  tbb::empty_task &SN_main_cont0_root =
      *new (tbb::task::allocate_root()) tbb::empty_task;
  SN_main_cont0_root.set_ref_count(2);
  main_cont0_task &SN_main_cont0 =
      *new (SN_main_cont0_root.allocate_child()) main_cont0_task(nullptr);
  SN_main_cont0.set_ref_count(1);
  fun_task &sp0 =
      *new (SN_main_cont0.allocate_child()) fun_task(&SN_main_cont0.n);
  sp0.n = 8;
  tbb::task::spawn(sp0);

  // Original sync was here
  SN_main_cont0_root.wait_for_all();
  tbb::task::destroy(SN_main_cont0_root);
  return 0;
}
tbb::task *fun_afterif0_task::execute() {
  w = (w + 40);
  w = (w * 10);
  w = (w - 6);
  SEND_ARGUMENT(bx_out, w);
  return nullptr;
}
tbb::task *fun_cont0_task::execute() {
  long f;
  long h;
  fun_cont1_task &SN_fun_cont1 =
      *new (allocate_continuation()) fun_cont1_task(bx_out);
  tbb::task_list SN_fun_cont1_list;
  int SN_fun_cont1_cnt = 0;
  fun_task &sp0 =
      *new (SN_fun_cont1.allocate_child()) fun_task(&SN_fun_cont1.f);
  sp0.n = (n - 5);
  SN_fun_cont1_list.push_back(sp0);
  ++SN_fun_cont1_cnt;

  fun_task &sp1 =
      *new (SN_fun_cont1.allocate_child()) fun_task(&SN_fun_cont1.h);
  sp1.n = (n - 2);
  SN_fun_cont1_list.push_back(sp1);
  ++SN_fun_cont1_cnt;

  SN_fun_cont1.y = y;
  SN_fun_cont1.x = x;
  SN_fun_cont1.n = n;
  // Original sync was here
  if (SN_fun_cont1_cnt > 0) {
    SN_fun_cont1.set_ref_count(SN_fun_cont1_cnt);
    tbb::task::spawn(SN_fun_cont1_list);
  } else {
    tbb::task::spawn(SN_fun_cont1);
  }
  return nullptr;
}
tbb::task *fun_cont1_task::execute() {
  long w;
  x = (3 + x);
  y = (y + 6);
  w = (((x + y) + f) + h);
  fun_afterif0_task &sp0 =
      *new (allocate_continuation()) fun_afterif0_task(bx_out);
  sp0.n = n;
  sp0.w = w;
  sp0.x = x;
  sp0.y = y;
  sp0.f = f;
  sp0.h = h;
  tbb::task::spawn(sp0);
  return nullptr;
}
tbb::task *main_cont0_task::execute() {
  printf("fun = %d\n", n);
  SEND_ARGUMENT(bx_out, 0);
  return nullptr;
}
