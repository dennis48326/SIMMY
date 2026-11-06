// @class EVec2
// 2D vector (inline arithmetic lived in headers; the out-of-line methods are below).
#pragma once

struct EVec2 {
  float x = 0.0f, y = 0.0f;
  EVec2() = default;
  constexpr EVec2(float x_, float y_) : x(x_), y(y_) {}

  void Print();  // 802D5A34

};
