// @template StackString2
// A UCS-2 string in a fixed buffer of N characters (game/stringbuffer2.h, StringBuffer2 over a stack
// array): what the game's string helpers (GetMoneyString, SubstituteString, ...) build into.
#pragma once

template <long N> struct StackString2 {
  unsigned short m_buf[N] = {};
  long m_len = 0;
  const unsigned short* c_str() const { return m_buf; }
  long length() const { return m_len; }
  void erase() {
    m_len = 0;
    m_buf[0] = 0;
  }
  // Appends up to `count` characters of `s` (all of it for count < 0), as far as the buffer goes.
  void append(const unsigned short* s, long count = -1) {
    for (long i = 0; s && s[i] && (count < 0 || i < count) && m_len < N - 1; ++i) m_buf[m_len++] = s[i];
    m_buf[m_len] = 0;
  }
};
