// @template TRect
// Axis-aligned rectangle (engine/e_rect.h): left, top, right, bottom.
#pragma once

template <class T> struct TRect {
  T x0{}, y0{}, x1{}, y1{};
  TRect() = default;
  constexpr TRect(T l, T t, T r, T b) : x0(l), y0(t), x1(r), y1(b) {}
  T Width() const { return x1 - x0; }
  T Height() const { return y1 - y0; }
};
