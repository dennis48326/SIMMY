// Port of game/wstringutil.cpp (partial): the UCS-2 helpers the HUD's money text needs: IntToWString,
// CopyCharStrToWString, SubstituteString (into a StackString2) and GetMoneyString; and
// UIObjectBase::makeMoneyString (game/uibase.cpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cstdio>
#include <cstring>

namespace {
unsigned short s_floatStrBuff[0x20];  // __floatstrbuff

std::size_t Len16(const unsigned short* s) {
  std::size_t n = 0;
  if (s)
    while (s[n]) ++n;
  return n;
}
int Compare16(const unsigned short* a, const unsigned short* b, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    if (!a[i]) return 0;
  }
  return 0;
}
// The text of a NewUIStrings row (EGlobal::GetNewUIString), or null.
const unsigned short* UIString(const char* name) {
  const unsigned short* const* handle = _globals.GetNewUIString(name);
  return handle ? *handle : nullptr;
}
const unsigned short k1000s[] = {'1', '0', '0', '0', 's', 0};  // _s1000s (EGlobal::SetupSubstitutionStrings)
const unsigned short k100s[] = {'1', '0', '0', 's', 0};        // _s100s
}  // namespace

// 8007AC88
// The number in decimal, zero-padded to `digits` when given.
void IntToWString(int value, unsigned short* out, unsigned int capacity, int digits) {
  char buf[0x20] = {};
  if (digits == 0) std::snprintf(buf, sizeof buf, "%d", value);
  else std::snprintf(buf, sizeof buf, "%0*d", digits, value);
  CopyCharStrToWString(buf, out, capacity);
}

// 8007AD28
void CopyCharStrToWString(char* in, unsigned short* out, unsigned int capacity) {
  unsigned int n = 0;
  if (*in && capacity != 1) {
    do {
      ++n;
      *out++ = static_cast<unsigned char>(*in++);
    } while (*in && n < capacity - 1);
  }
  *out = 0;
}

// 8007AD78
void CopyWStringToCharStr(unsigned short* in, char* out, unsigned int capacity) {
  unsigned int n = 0;
  if (*in && capacity != 1) {
    do {
      ++n;
      *out++ = static_cast<char>(*in++);
    } while (*in && n < capacity - 1);
  }
  *out = 0;
}

// 8007ADC8
// `text` with the first "<key>" in it replaced by `with`, into `out`; 1 when replaced. Without one
// `out` is a copy of `text` (0).
RET_UNKNOWN SubstituteString(unsigned short* text, unsigned short* key, unsigned short* with, StackString2<256>& out) {
  StackString2<256> src;
  src.append(text);
  const long length = src.length();
  const std::size_t keyLen = Len16(key);
  out.erase();
  auto find = [&](unsigned short c, long from) -> long {
    for (long i = from; i < length; ++i)
      if (src.m_buf[i] == c) return i;
    return -1;
  };
  long l = find('<', 0), r = find('>', 0);
  if (l >= 0 && r >= 0 && l < r) {
    for (;;) {
      const std::size_t n = static_cast<std::size_t>(r - l - 1);
      if (n != 0 && n == keyLen && !Compare16(src.m_buf + l + 1, key, n)) {
        if (l) out.append(src.c_str(), l);
        out.append(with);
        if (r + 1 < length) out.append(src.m_buf + r + 1);
        return 1;
      }
      l = find('<', r + 1);
      r = find('>', r + 1);
      if (l < 0 || r < 0 || r <= l) break;
    }
  }
  out.erase();
  out.append(src.c_str());
  return 0;
}

// 8007B33C
// The time by the UI string "time format": "<min>" the minute (two digits), "<hours24>" the hour (two
// digits) or, when the format has "am-pm", "<hour>" on the 12-hour clock and "<am-pm>" pm / am; ":" is
// the "time seperator" UI string.
void GetTimeString(int hour, int minute, unsigned short*, StackString2<256>& out) {
  static const unsigned short kHour[] = {'h', 'o', 'u', 'r', 0};                  // _sHoursLookup
  static const unsigned short kHours24[] = {'h', 'o', 'u', 'r', 's', '2', '4', 0};  // _sHours24Lookup
  static const unsigned short kMin[] = {'m', 'i', 'n', 0};                         // _sMinLookup (803D251C)
  static const unsigned short kAmPm[] = {'a', 'm', '-', 'p', 'm', 0};              // _sAM_PMLookup
  static const unsigned short kColon[] = {':', 0};                                 // _sColon (803D2584)
  static const unsigned short kPm[] = {'p', 'm', 0}, kAm[] = {'a', 'm', 0};        // 803D6714 / 803D671C
  StackString2<256> layout;
  layout.append(UIString("time format"));
  bool h24 = true, pm = false;
  for (long i = 0; i + 5 <= layout.length(); ++i)
    if (Compare16(layout.c_str() + i, kAmPm, 5) == 0) {
      pm = hour > 11;
      hour %= 12;
      if (hour == 0) hour = 12;
      h24 = false;
      break;
    }
  out.erase();
  s_floatStrBuff[0] = 0;
  IntToWString(minute, s_floatStrBuff, 0x20, 2);
  SubstituteString(const_cast<unsigned short*>(layout.c_str()), const_cast<unsigned short*>(kMin), s_floatStrBuff, out);
  s_floatStrBuff[0] = 0;
  StackString2<256> step;
  step.append(out.c_str());
  IntToWString(hour, s_floatStrBuff, 0x20, h24 ? 2 : 0);
  SubstituteString(const_cast<unsigned short*>(step.c_str()), const_cast<unsigned short*>(h24 ? kHours24 : kHour),
                   s_floatStrBuff, out);
  s_floatStrBuff[0] = 0;
  step.erase();
  step.append(out.c_str());
  const unsigned short* sep = UIString("time seperator");
  static const unsigned short kNone[] = {0};
  SubstituteString(const_cast<unsigned short*>(step.c_str()), const_cast<unsigned short*>(kColon),
                   const_cast<unsigned short*>(sep ? sep : kNone), out);
  if (!h24) {
    step.erase();
    step.append(out.c_str());
    SubstituteString(const_cast<unsigned short*>(step.c_str()), const_cast<unsigned short*>(kAmPm),
                     const_cast<unsigned short*>(pm ? kPm : kAm), out);
  }
}

// 8007B5F4
// The amount (0..999999; a negative one with '-') laid out by the UI strings
// "money_less_than_1000" / "money_greater_than_999" (or their "_no_symbol" forms without `symbol`):
// "<1000s>" the thousands, "<100s>" the rest (three digits after thousands).
void GetMoneyString(int amount, StackString2<256>& out, bool symbol) {
  bool negative = false;
  if (amount < 0) {
    negative = true;
    amount = -amount;
    if (amount < 0) amount = 0;
  }
  if (amount > 999999) amount = 999999;
  const unsigned short* layout = UIString(amount < 1000 ? "money_less_than_1000" : "money_greater_than_999");
  if (!layout) return;
  if (!symbol) {
    layout = UIString(amount < 1000 ? "money_less_than_1000_no_symbol" : "money_greater_than_999_no_symbol");
    if (!layout) return;
  }
  const int rest = amount % 1000;
  s_floatStrBuff[0] = 0;
  IntToWString(amount / 1000, s_floatStrBuff, 0x20, 0);
  StackString2<256> text;
  if (negative) {
    unsigned short minus[2] = {'-', 0};  // DAT_803D6798
    text.append(minus);
  }
  text.append(layout);
  SubstituteString(const_cast<unsigned short*>(text.c_str()), const_cast<unsigned short*>(k1000s), s_floatStrBuff, out);
  s_floatStrBuff[0] = 0;
  if (amount / 1000 == 0 && rest < 100) IntToWString(rest, s_floatStrBuff, 0x20, rest < 10 ? 1 : 2);
  else IntToWString(rest, s_floatStrBuff, 0x20, 3);
  StackString2<256> withThousands;
  withThousands.append(out.c_str());
  SubstituteString(const_cast<unsigned short*>(withThousands.c_str()), const_cast<unsigned short*>(k100s), s_floatStrBuff,
                   out);
  s_floatStrBuff[0] = 0;
}

// 80177C30
bool UIObjectBase::makeMoneyString(int amount, unsigned short* out, bool symbol) {
  if (!out) return false;
  StackString2<256> text;
  GetMoneyString(amount, text, symbol);
  std::memcpy(out, text.c_str(), (static_cast<std::size_t>(text.length()) + 1) * 2);
  return true;
}
