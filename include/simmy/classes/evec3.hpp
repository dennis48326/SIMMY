// @class EVec3
// 3D vector (base/e_vec3.cpp). Arithmetic was inline in EA's headers; add operators here as ported
// code needs them.
#pragma once

struct EVec3 {
  float x = 0.0f, y = 0.0f, z = 0.0f;
  EVec3() = default;
  constexpr EVec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  float& operator[](int i) { return (&x)[i]; }
  const float& operator[](int i) const { return (&x)[i]; }

  void Print();  // 802D5A6C
  void ToU8s(unsigned char* out) const;  // 802D5AA8
  void FromU8s(unsigned char* in);  // 802D5B20
  void ToS8s(signed char* out) const;  // 802D5B78
  void FromS8s(signed char* in);  // 802D5BF0

};
