// Port of game/widgetsupport.cpp: WidgetScreenFormat, which turns a widget layout into the XML
// document s2c_root's widget manager builds the screen from.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstring>

namespace {
// ValidTagsAndTests (803E3F58): the tags a layout may use (the tests check values; IsDataValid).
const char* const kValidTags[] = {
    "<x>", "<y>", "<h>", "<w>", "<z>", "<vis>", "<strvar>", "<btnname>", "<type>", "<clr>", "<fnt>", "<strans>",
    "<htrans>", "<txtalgn>", "<style>", "<plyr>", "<alpha>", "<scrnid>", "<swfid>", "<shdrid>", "<setvar>",
    "<swffn>", "<rot>", "<scrn>", "<subscrn>", "<lbox>", "<spane>", "<txt>", "<btn>", "<shdr>", "<swf>", "<msk>",
    "<arwup>", "<arwdn>", "<scrlbg>", "<scrlthm>", "<txtid>", "<mskid>", "<subscrnid>", "<spaneid>", "<lboxid>",
};
}  // namespace

// Defaulted in uiscreen.hpp (the document is a std::string):
// 8018230C WidgetScreenFormat::WidgetScreenFormat
// 80182328 WidgetScreenFormat::~WidgetScreenFormat

// 8018237C
// Measures, builds and sends the document ("LoadWidget <xml> <input mode>").
void WidgetScreenFormat::LaunchWidgetScreenLayout(const char* const* layout, int player, int inputMode) {
  m_player = player;
  IterateScreenLayout(layout, kMeasure);
  m_out.clear();
  m_out.reserve(m_size + 1);
  IterateScreenLayout(layout, kBuild);
  const char mode[2] = {static_cast<char>('0' + inputMode), 0};
  if (auto* v = static_cast<AptViewer*>(_globals.m_aptViewer))
    v->NewCallFunction(const_cast<char*>("cmdProc"), nullptr, nullptr, 3, "LoadWidget", m_out.c_str(), mode);
  m_out.clear();
}

// 80182458
bool WidgetScreenFormat::IterateScreenLayout(const char* const* layout, int type) {
  if (std::strcmp(layout[0], "<scrn>") != 0) return false;
  m_pos = layout;
  m_result = 0;
  if (type == kMeasure) m_size = 0;
  IterateTags(type);
  return m_result == 1;
}

// 801824D8
// One element: its tag, its values and children up to the matching closing tag.
void WidgetScreenFormat::IterateTags(int type) {
  const char* tag = GetNextTag(type);
  if (!IsValidTag(tag)) {
    m_result = 2;
    return;
  }
  if (type == kMeasure) m_size += std::strlen(tag);
  else if (type == kBuild) m_out += tag;
  ++m_pos;
  if (std::strncmp(*m_pos, "</", 2) == 0) {
    m_result = 2;
    return;
  }
  std::string close = "</";
  close += tag + 1;
  const char* next = tag;
  while (true) {
    if (m_result == 2) return;
    m_tag = next;
    next = GetNextTag(type);
    if (close == next) break;
    IterateTags(type);
  }
  if (type == kBuild) m_out += close;
  else if (type == kMeasure) m_size += close.size();
  if (close == "</scrn>") {
    m_result = 1;
    return;
  }
  ++m_pos;
  if (std::strncmp(*m_pos, "<", 1) != 0) m_result = 2;
}

// 801826D4
// Copies (or measures) the values up to the next tag: "<plyr>" becomes "0", "getloc:" string
// variables and the ids get the player's digit.
const char* WidgetScreenFormat::GetNextTag(int type) {
  for (int values = 0;; ++values) {
    if (std::strncmp(*m_pos, "<", 1) == 0 || values > 1) {
      if (values > 1) m_result = 2;
      return *m_pos;
    }
    m_value = *m_pos;
    const char digit = static_cast<char>('0' + m_player);
    const bool getloc = std::strcmp(m_tag, "<strvar>") == 0 && std::strncmp(m_value, "getloc:", 7) == 0;
    if (type == kMeasure) {
      m_size += std::strlen(m_value);
      if (getloc || (std::strcmp(m_tag, "<strvar>") != 0 && IsPlayerSpecificTag(m_tag))) ++m_size;
    } else if (type == kBuild) {
      if (std::strcmp(m_tag, "<plyr>") == 0) {
        m_out += '0';
      } else if (getloc) {
        m_out += "getloc:";
        m_out += digit;
        m_out += m_value + 7;
      } else if (std::strcmp(m_tag, "<strvar>") != 0 && IsPlayerSpecificTag(m_tag)) {
        m_out += digit;
        m_out += m_value;
      } else {
        m_out += m_value;
      }
    }
    ++m_pos;
  }
}

// 80182904
bool WidgetScreenFormat::IsValidTag(const char* tag) {
  for (const char* t : kValidTags)
    if (std::strcmp(t, tag) == 0) return true;
  return false;
}

// 80182E3C
bool WidgetScreenFormat::IsPlayerSpecificTag(const char* tag) {
  for (const char* t : {"<scrnid>", "<swfid>", "<shdrid>", "<subscrnid>", "<txtid>", "<mskid>", "<lboxid>", "<setvar>"})
    if (std::strcmp(t, tag) == 0) return true;
  return false;
}
