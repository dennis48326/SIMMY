// @class EVec4
// 4D vector / RGBA colour (engine/e_vec4.cpp).
#pragma once

struct EVec4 {
  float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
  EVec4() = default;
  constexpr EVec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
  float& operator[](int i) { return (&x)[i]; }
  const float& operator[](int i) const { return (&x)[i]; }

  void Print();  // 80361F34
  void ToU8s(unsigned char* out) const;  // 80361F38
  void FromU8s(unsigned char* in);  // 80361FB0

};
