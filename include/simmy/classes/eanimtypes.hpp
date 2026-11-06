// @class EAnimNodeDataPos
// @class EAnimNote
// @class EAnimEvent
// Elements of an animation (engine/e_ranim.cpp). Values are stored big-endian in ANIM data and
// converted on load.
#pragma once
#include <string>

struct EAnimNodeDataPos {
  unsigned int m_field00 = 0;  // +0x00 (not stored)
  unsigned int m_data[8] = {};  // +0x04 per-node track positions/flags
};

struct EAnimNote {
  unsigned int m_time = 0;     // +0x00
  unsigned int m_field04 = 0;  // +0x04
  std::string m_text;          // +0x08 EString
};

struct EAnimEvent {
  char m_name[0x40] = {};      // +0x00
  unsigned int m_data[4] = {}; // +0x40
};
