// @class eTattooTextureTypeS2C
// @class Sim__SleeveStyleS2C
// @class Sim__MidLayerTorsoStyleS2C
// CAS enums (game/cassimparts.cpp, cassimstate.cpp): the tattoo places and the sleeve and mid-layer
// torso styles.
#pragma once

struct eTattooTextureTypeS2C {
  SIMMY_ENUM_BODY(eTattooTextureTypeS2C)
  enum : int { kHeadArms = 0, kTorso = 1, kLegs = 2 };
};
// 0 sleeveless .. 3 long.
struct Sim__SleeveStyleS2C {
  SIMMY_ENUM_BODY(Sim__SleeveStyleS2C)
};
// 0..2 full torso, 3..5 their half (under an outer top) versions.
struct Sim__MidLayerTorsoStyleS2C {
  SIMMY_ENUM_BODY(Sim__MidLayerTorsoStyleS2C)
};
