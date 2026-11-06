// Port of engine/e_ngcraudiosample.cpp: ERSampledata, a DSP-ADPCM sample (see ersampledata.hpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80338114
ERSampledata::ERSampledata() = default;

// 8033815C
// (GC: the frames were freed from the ARAM heap by OnDelRef; on PC they are host memory.)
ERSampledata::~ERSampledata() {
  _ngcAudioSampleMan.OnDelRef(this);
  delete[] m_header;
  m_header = nullptr;
  delete[] m_header2;
  m_header2 = nullptr;
}

namespace {
// Reads a 0x60-byte header; an odd nibble count is rounded up to whole bytes.
unsigned char* ReadHeader(EFile* file) {
  auto* header = new unsigned char[ERSampledata::kHeaderSize];
  file->Read(header, ERSampledata::kHeaderSize);
  const unsigned int nibbles = simmy::be32(header + 4);
  if (nibbles & 1) {
    const unsigned int even = nibbles + 1;
    for (int i = 0; i < 4; ++i) header[4 + i] = static_cast<unsigned char>(even >> (24 - 8 * i));
  }
  return header;
}
}  // namespace

// 803381E8
void ERSampledata::Load(EFile* file) { m_header = ReadHeader(file); }
// 80338258
void ERSampledata::LoadStereo(EFile* file) { m_header2 = ReadHeader(file); }

// 80338324
const unsigned char* ERSampledata::GetSampleHeader() const { return m_header; }
// 8033832C
const unsigned char* ERSampledata::GetSampleHeader2() const { return m_header2; }
