// @class ENeighborhoodCustomChar after CasSimDescriptionS2C
// The sims made in Create-a-Sim on their way into the neighbourhood (game/neighbor.cpp's
// ENeighborhoodCustomChar), and the table of the four family slots that EGlobal::SetDefaults makes
// and GameData keeps (GC: GameData +0x78, DAT_80475BFC). CASTarget::SaveSimToFamilySlot fills a
// slot; the neighbourhood (Neighbor::LoadFromCharacter, GameData::StoreCreateAFamilyData) reads it.
#pragma once
#include <vector>

// 0x188 bytes.
struct ENeighborhoodCustomChar {
  ENeighborhoodCustomChar();  // 800C8A58

  // +0x00 the personality points x100 (nice, active, +0x04, playful, outgoing, neat)
  short m_nice = 0;                    // +0x00
  short m_active = 0;                  // +0x02
  short m_field04 = 0;                 // +0x04
  short m_playful = 0;                 // +0x06
  short m_outgoing = 0;                // +0x08
  short m_neat = 0;                    // +0x0A
  unsigned char m_aspiration = 0;      // +0x0C
  int m_field10 = 0;                   // +0x10
  CasSimDescriptionS2C m_desc;         // +0x14 (names at +0x18 and +0x58)
  void* m_faceImage = nullptr;         // +0x138 the portrait (CasSimRenderer::CreateFaceImage)
  int m_field13C[14] = {};             // +0x13C
  int m_field174 = 0;                  // +0x174
  int m_field178[4] = {};              // +0x178
};

// 0x654 bytes (GC: operator new(0x654) in EGlobal::SetDefaults).
struct NeighborhoodCustomChars {
  ENeighborhoodCustomChar m_chars[4];        // +0x000
  int m_used[4] = {};                        // +0x620 the slot holds a sim made in CAS
  int m_field630[4] = {};                    // +0x630
  unsigned int m_neighborIds[4] = {};        // +0x640 the neighbour each slot replaces
  std::vector<unsigned short> m_familyName;  // +0x650 (GC: a StringBuffer2* of 0x100 characters)
};
