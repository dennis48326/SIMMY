// @class ERAnim after EResource EAnimNodeDataPos EAnimNote EAnimEvent EBitArray
// An animation resource (engine/e_ranim.cpp), 'ANIM' versions 0x11-0x12.
#pragma once
#include <vector>

struct ERAnim : public EResource {
  ERAnim();
  ~ERAnim() override;
  SIMMY_STORABLE_DECL(ERAnim)
  void Load(EFile* file);
  using EResource::Load;
  void Load(EFile& file) override { Load(&file); }
  void OldLoad(EFile*);
  void Refresh(EFile* file) override;
  using EResource::Refresh;
  static void RegisterType(unsigned short);
  void CreateCopy() const;

  static int m_allocatedAnims;  // 804FF434

  int m_durationMs = 0;              // +0x14
  int m_frameCount = 0;              // +0x18
  float m_field1C[3] = {1, 1, 1};    // +0x1C
  float m_field28[3] = {1, 1, 1};    // +0x28
  std::vector<EAnimNodeDataPos> m_nodes;  // +0x34
  unsigned int m_field40 = 0;        // +0x40
  std::vector<float> m_constants;    // +0x44
  EBitArray m_bitArray;              // +0x50 the compressed key streams (EAnimController's decompressors)
  float m_fps = 60.0f;               // +0x64
  float m_field68 = 1.0f;            // +0x68
  unsigned int m_field6C = 0xfb;     // +0x6C
  unsigned char m_field70 = 3, m_field71 = 0, m_field72 = 1;  // +0x70
  unsigned int m_field74 = 0, m_field78 = 0;  // +0x74
  float m_field7C = 0.5f, m_field80 = 0.08f, m_field84 = 0.01f;  // +0x7C
  std::vector<EAnimNote> m_notes;    // +0x88
  std::vector<EAnimEvent> m_events;  // +0x94
};
