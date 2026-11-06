// Logging for unported functions. Each one logs once, with its original address, so the log
// shows exactly where the port has to go next.
#include "simmy/port.hpp"

#include <cstdio>
#include <mutex>
#include <unordered_set>

namespace simmy {

// Function-local state: stubs can run from static constructors, before this file's globals exist.
struct StubState {
  std::mutex mutex;
  std::unordered_set<std::uint32_t> seen;
  unsigned count = 0;
};
static StubState& state() {
  static StubState s;
  return s;
}

void stub_hit(const char* name, std::uint32_t addr) {
  StubState& s = state();
  std::lock_guard lock(s.mutex);
  if (!s.seen.insert(addr).second) {
    return;
  }
  ++s.count;
  std::fprintf(stderr, "[stub #%u] %08X %s\n", s.count, addr, name);
}

unsigned stub_unique_count() {
  StubState& s = state();
  std::lock_guard lock(s.mutex);
  return s.count;
}

} // namespace simmy
