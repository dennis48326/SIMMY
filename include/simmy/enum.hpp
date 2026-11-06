// Enum wrappers. The map gives enum *names* but not their definitions, and the generator has already
// forward-declared every type as a struct. A recovered enum is therefore a struct holding an int,
// convertible to and from int, with the known values as named constants.
#pragma once

#define SIMMY_ENUM_BODY(Name)                                   \
  int value = 0;                                                \
  constexpr Name() = default;                                   \
  constexpr Name(int v) : value(v) {}                           \
  constexpr operator int() const { return value; }
