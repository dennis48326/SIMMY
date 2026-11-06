// Port of game/recon.cpp (part): the ReconBuffer's floats and strings, and ReconBuilder::Compact (an
// object streamed into a new resource). The rest of the ReconBuffer and ReconBuilder::Reconstitute
// are in world.cpp.
//
// The string table (ReconBuffer::UseStringTable: strings kept in a resource of their own, the
// buffer's +0x14 flag) is not ported; no save stream uses it.
#include <cstdio>
#include <cstdlib>
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>
#include <vector>
#if defined(__linux__)
#include <execinfo.h>
#endif

extern std::vector<int>* g_reconOpTrace;  // world.cpp
extern int g_reconOpStop;
void ReconTraceOp(const ReconBuffer& b);

// PC dev aid: the caller chain (Linux: addresses for addr2line; elsewhere nothing).
void PcPrintBacktrace() {
#if defined(__linux__)
  void* frames[24];
  const int n = backtrace(frames, 24);
  backtrace_symbols_fd(frames, n, 2);
#endif
}

namespace {
// Characters of a UTF-16 string in a stream, from `p`, `big`-endian, up to the terminating 0.
std::u16string ReadUtf16(const unsigned char* p, bool big) {
  std::u16string s;
  for (;; p += 2) {
    const auto c = static_cast<char16_t>(big ? p[0] << 8 | p[1] : p[1] << 8 | p[0]);
    if (!c) break;
    s.push_back(c);
  }
  return s;
}

void AlignCursor(ReconBuffer& b) {
  if (b.m_pos & 1) ++b.m_pos;
}

// Written: 0xFFFE then the characters big-endian (GC: wcscpy of its native 16-bit characters).
void WriteUtf16(ReconBuffer& b, const char16_t* s, size_t n) {
  unsigned char* p = b.m_data + b.m_pos;
  p[0] = 0xff, p[1] = 0xfe;
  for (size_t i = 0; i <= n; ++i) {
    const char16_t c = i < n ? s[i] : 0;
    p[2 + i * 2] = static_cast<unsigned char>(c >> 8);
    p[3 + i * 2] = static_cast<unsigned char>(c);
  }
}

// What a UTF-16 string field holds when read: the text and whether it was UTF-16 (else narrow).
std::u16string ReadUtf16Field(ReconBuffer& b, bool& wide) {
  const unsigned char* p = b.m_data + b.m_pos;
  const unsigned int mark = static_cast<unsigned int>(p[0] << 8 | p[1]);
  wide = mark == 0xfffe || mark == 0xfeff;
  if (wide) return ReadUtf16(p + 2, mark == 0xfffe);
  std::u16string s;
  for (; *p; ++p) s.push_back(static_cast<char16_t>(*p));
  return s;
}
}  // namespace

// 801337E0
// Like Recon32 on the floats' bits (ReconCmprInt with the 32-bit scheme when compressed).
void ReconBuffer::ReconFloat(float* v, int count) {
  for (int i = 0; i < count; ++i) {
    int bits = 0;
    std::memcpy(&bits, &v[i], 4);
    Recon32(&bits, 1);
    if (m_mode == ReconBuffer__Mode::kRead) std::memcpy(&v[i], &bits, 4);
  }
}

// 80133AE4
// A narrow string and its 0, the cursor then made even.
void ReconBuffer::ReconString(std::string& s) {
  ReconTraceOp(*this);
  if (m_compressed) PadBits();
  if (m_mode == ReconBuffer__Mode::kRead) s = reinterpret_cast<const char*>(m_data + m_pos);
  else if (m_mode == ReconBuffer__Mode::kWrite) std::memcpy(m_data + m_pos, s.c_str(), s.size() + 1);
  m_pos += 1 + static_cast<int>(s.size());
  AlignCursor(*this);
}

// 80133C48
// At an even position: 0xFFFE and the UTF-16 characters (big-endian, as the GameCube wrote them) and a
// 0; 0xFEFF before little-endian ones; anything else is a narrow string. The cursor ends even.
void ReconBuffer::ReconString(std::u16string& s) {
  ReconTraceOp(*this);
  if (m_compressed) PadBits();
  AlignCursor(*this);
  if (m_mode == ReconBuffer__Mode::kRead) {
    bool wide = false;
    s = ReadUtf16Field(*this, wide);
    m_pos += wide ? (static_cast<int>(s.size()) + 2) * 2 : 1 + static_cast<int>(s.size());
  } else {
    if (m_mode == ReconBuffer__Mode::kWrite) WriteUtf16(*this, s.data(), s.size());
    m_pos += (static_cast<int>(s.size()) + 2) * 2;
  }
  AlignCursor(*this);
}

// 80133ED0
// As the BString one, into a buffer of `capacity` characters (the text cut to fit).
void ReconBuffer::ReconString(char* buffer, int capacity) {
  ReconTraceOp(*this);
  if (m_compressed) PadBits();
  if (m_mode == ReconBuffer__Mode::kRead) {
    std::strncpy(buffer, reinterpret_cast<const char*>(m_data + m_pos), static_cast<size_t>(capacity - 1));
    buffer[capacity - 1] = 0;
  } else if (m_mode == ReconBuffer__Mode::kWrite) {
    std::memcpy(m_data + m_pos, buffer, std::strlen(buffer) + 1);
  }
  m_pos += 1 + static_cast<int>(std::strlen(buffer));
  AlignCursor(*this);
}

// 80134028
// As the BString2 one, into a buffer of `capacity` characters (the text cut to fit).
void ReconBuffer::ReconString(unsigned short* buffer, int capacity) {
  ReconTraceOp(*this);
  if (m_compressed) PadBits();
  AlignCursor(*this);
  if (m_mode == ReconBuffer__Mode::kRead) {
    bool wide = false;
    std::u16string s = ReadUtf16Field(*this, wide);
    if (static_cast<int>(s.size()) > capacity - 1) s.resize(static_cast<size_t>(capacity - 1));
    for (size_t i = 0; i < s.size(); ++i) buffer[i] = s[i];
    buffer[s.size()] = 0;
    m_pos += wide ? (static_cast<int>(s.size()) + 2) * 2 : 1 + static_cast<int>(s.size());
  } else {
    std::u16string s;
    for (int i = 0; i < capacity && buffer[i]; ++i) s.push_back(static_cast<char16_t>(buffer[i]));
    if (m_mode == ReconBuffer__Mode::kWrite) WriteUtf16(*this, s.data(), s.size());
    m_pos += (static_cast<int>(s.size()) + 2) * 2;
  }
  AlignCursor(*this);
}

// ---------------------------------------------------------------------------------------------------
// ReconBuilder

// 8013426C
// Measured, then written after the header (GC: the header's words swapped to little-endian).
Memory__HandleNode* ReconBuilder::Compact(ReconObject* object, int version) {
  static const bool trace = std::getenv("SIMMY_RECON_TRACE") != nullptr;
  std::vector<int> first;
  if (trace) g_reconOpTrace = &first;
  ReconBuffer measure(nullptr, 0, ReconBuffer__Mode{ReconBuffer__Mode::kMeasure}, false);
  object->DoStream(&measure, version);
  g_reconOpTrace = nullptr;
  auto* handle = new Memory__HandleNode;
  handle->m_size = static_cast<unsigned int>(measure.m_pos + 0xc);
  handle->m_data = new unsigned char[handle->m_size]();
  handle->m_owned = 1;
  unsigned char* h = handle->m_data;
  const unsigned int type = static_cast<unsigned int>(object->GetType());
  for (int i = 0; i < 4; ++i) {
    h[4 + i] = static_cast<unsigned char>(static_cast<unsigned int>(version) >> (8 * i));
    h[8 + i] = static_cast<unsigned char>(type >> (8 * i));
  }
  ReconBuffer write(h + 0xc, measure.m_pos, ReconBuffer__Mode{ReconBuffer__Mode::kWrite}, false);
  object->DoStream(&write, version);
  if (write.m_pos != measure.m_pos && std::getenv("SIMMY_RECON_TRACE")) {
    std::fprintf(stderr, "[recon] %08x: measured %d bytes, wrote %d\n", type, measure.m_pos, write.m_pos);
    // PC dev aid: the first value op whose position differs between a measure and a write pass, and
    // the caller chain of the op before it (the one whose size differs).
    std::vector<int> a, b;
    g_reconOpTrace = &a;
    ReconBuffer m2(nullptr, 0, ReconBuffer__Mode{ReconBuffer__Mode::kMeasure}, false);
    object->DoStream(&m2, version);
    std::vector<unsigned char> scratch(static_cast<size_t>(write.m_pos) + 0x2000);
    g_reconOpTrace = &b;
    ReconBuffer w2(scratch.data(), static_cast<int>(scratch.size()), ReconBuffer__Mode{ReconBuffer__Mode::kWrite}, false);
    object->DoStream(&w2, version);
    g_reconOpTrace = nullptr;
    size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
    if (i == a.size() && i == b.size()) {
      // The passes after the first agree: the first one differs (it changed what the object streams).
      b = first;
      i = 0;
      while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
      std::fprintf(stderr, "[recon] the first measure pass: %zu ops, first op differing from the second's %zu (at %d / %d)\n",
                   b.size(), i, i < b.size() ? b[i] : -1, i < a.size() ? a[i] : -1);
      if (i > 0 && i < a.size()) {
        g_reconOpStop = static_cast<int>(i) - 1;
        std::vector<int> c;
        g_reconOpTrace = &c;
        std::fprintf(stderr, "[recon] op %zu (at %d) now:\n", i - 1, a[i - 1]);
        ReconBuffer m4(nullptr, 0, ReconBuffer__Mode{ReconBuffer__Mode::kMeasure}, false);
        object->DoStream(&m4, version);
        g_reconOpTrace = nullptr;
        g_reconOpStop = -1;
      }
      i = a.size();
    }
    std::fprintf(stderr, "[recon] second passes: measured %d, wrote %d; %zu / %zu ops, first differing op %zu (at %d / %d)\n",
                 m2.m_pos, w2.m_pos, a.size(), b.size(), i, i < a.size() ? a[i] : -1, i < b.size() ? b[i] : -1);
    if (i > 0 && i < a.size()) {
      std::vector<int> c;
      g_reconOpTrace = &c;
      g_reconOpStop = static_cast<int>(i) - 1;
      std::fprintf(stderr, "[recon] op %zu (at %d) in the measure pass:\n", i - 1, a[i - 1]);
      ReconBuffer m3(nullptr, 0, ReconBuffer__Mode{ReconBuffer__Mode::kMeasure}, false);
      object->DoStream(&m3, version);
      std::vector<unsigned char> scratch2(scratch.size());
      std::vector<int> d;
      g_reconOpTrace = &d;
      std::fprintf(stderr, "[recon] ... and in the write pass:\n");
      ReconBuffer w3(scratch2.data(), static_cast<int>(scratch2.size()), ReconBuffer__Mode{ReconBuffer__Mode::kWrite}, false);
      object->DoStream(&w3, version);
      g_reconOpTrace = nullptr;
      g_reconOpStop = -1;
    }
  }
  if (write.m_pos > measure.m_pos) {
    // PC: a stream must write what it measured (session 22's 2 bytes: person data 0x3A / 0x41, now set
    // in cXPersonImpl::Initialize as on GC). More would have overrun the handle.
    std::fprintf(stderr, "[recon] %08x: wrote %d bytes, measured %d\n", type, write.m_pos, measure.m_pos);
    std::abort();
  }
  return handle;
}

// 801343D4
// GC: then the file's Write (a no-op for the neighbourhood file), Remove on its error.
int ReconBuilder::Compact(ReconObject* object, int version, iResFile* file, short id) {
  Memory__HandleNode* handle = Compact(object, version);
  if (!handle) return -1;
  static StringBuffer noName;
  file->Add(handle, object->GetType(), id, noName, false);
  return file->GetError();
}
