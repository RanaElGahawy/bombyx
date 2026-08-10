#ifndef HLS_STREAM_H
#define HLS_STREAM_H
#include <deque>
#include <cstddef>
namespace hls {
template <typename T> class stream {
  std::deque<T> q;
public:
  stream() {}
  stream(const char *) {}
  T read() { T t = q.front(); q.pop_front(); return t; }
  void read(T &t) { t = read(); }
  bool read_nb(T &t) { if (q.empty()) return false; t = read(); return true; }
  void write(const T &t) { q.push_back(t); }
  bool write_nb(const T &t) { q.push_back(t); return true; }
  bool empty() const { return q.empty(); }
  bool full() const { return false; }
  size_t size() const { return q.size(); }
};
} // namespace hls
#endif
