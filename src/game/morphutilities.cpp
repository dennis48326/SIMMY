// Port of game/morphutilities.cpp: the CAS body shape and face sliders as lattice weights on a
// sim's parts (see MorphUtilities in simmodel.hpp).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

#include <cmath>

namespace {
ERModel* PartModel(EIStaticModel** parts, unsigned int part) {
  EIStaticModel* m = parts[part];
  return m ? m->m_model : nullptr;
}
}  // namespace

// 80057C18
MorphUtilities::MorphUtilities(EIStaticModel** parts) : m_parts(parts) {}

// 80057C20
MorphUtilities::~MorphUtilities() { m_parts = nullptr; }

// 80057C50
void MorphUtilities::ApplyAllMorphs(CasSimDescriptionS2C& desc) {
  ApplyMorph(0, desc.m_morphBody);
  ApplyMorph(1, desc.m_morphArms);
  ApplyMorph(3, desc.m_morphLegs);
  ApplyMorphHead(desc.m_morphHead);
}

// 80057CB4
// The parts each slider shapes, and with which of their lattices.
void MorphUtilities::ApplyMorph(int what, float value) {
  if (what == 1) {  // arms: the sleeves and the arm accessory, both lattices
    for (unsigned int lattice = 0; lattice < 2; ++lattice)
      for (unsigned int part : {8u, 11u, 14u, 21u}) MorphLatticeOnBodyPart(part, value, lattice);
  } else if (what == 0) {  // body
    for (unsigned int part : {1u, 7u, 9u}) MorphLatticeOnBodyPart(part, value, 1);
    MorphLatticeOnBodyPart(12, value, 0);
    MorphLatticeOnBodyPart(20, value, 1);
    MorphLatticeOnBodyPart(24, value, 1);
    for (unsigned int part : {10u, 13u, 22u, 23u, 25u, 15u}) MorphLatticeOnBodyPart(part, value, 0);
  } else if (what == 3) {  // legs (each call twice, as on the GC)
    MorphLatticeOnBodyPart(15, value, 1);
    MorphLatticeOnBodyPart(15, value, 1);
    MorphLatticeOnBodyPart(17, value, 0);
    MorphLatticeOnBodyPart(17, value, 0);
  }
}

// 80057EF4
// The head shape on the head and everything worn on it.
void MorphUtilities::ApplyMorphHead(float value) {
  for (unsigned int part : {0u, 2u, 3u, 4u, 5u, 6u, 18u, 19u, 1u, 7u, 9u, 20u, 24u}) MorphLatticeOnHead(part, value, 0);
}

// 80058024
// Head parts (glasses, head, hair, inner/mid torso, outer torso, facial feature, hat, necklace,
// belt extra) with at least 7 volumes in `lattice`: the value's two volumes get its weights, the
// others 0.
void MorphUtilities::MorphLatticeOnHead(unsigned int part, float value, unsigned int lattice) {
  const bool headPart = part <= 7 || part == 9 || part == 12 || (part >= 18 && part <= 20) || part == 24;
  if (!headPart) return;
  ERModel* model = PartModel(m_parts, part);
  if (!model || model->GetNumMorphBases() <= 0 || model->GetNumMorphLattice(static_cast<int>(lattice)) <= 6) return;
  if (value < 0.0f) value = 0.0f;
  if (7.0f < value) value = 7.0f;
  int i1 = 0, i2 = -1;
  float w1 = 0.0f, w2 = 0.0f;
  GetHeadLatticeIndexAndValue(value, w1, i1, w2, i2);
  const auto fits = [&](int volume) {
    return lattice < static_cast<unsigned int>(model->GetNumMorphBases()) &&
           volume < model->GetNumMorphLattice(static_cast<int>(lattice));
  };
  if (i1 > -1 && fits(i1)) model->SetLatticeWeight(static_cast<int>(lattice), i1, w1);
  if (i2 > -1 && fits(i2)) model->SetLatticeWeight(static_cast<int>(lattice), i2, w2);
  for (int v = 0; v < 7; ++v)
    if (v != i1 && v != i2 && fits(v)) model->SetLatticeWeight(static_cast<int>(lattice), v, 0.0f);
}

// 80058224
// The same for a body slider (3 volumes); 1 if a weight was set.
int MorphUtilities::MorphLatticeOnBodyPart(unsigned int part, float value, unsigned int lattice) {
  ERModel* model = PartModel(m_parts, part);
  if (!model) return 0;
  int i1 = 0, i2 = -1, result = 0;
  float w1 = 0.0f, w2 = 0.0f;
  GetBodyLatticeIndexAndValue(value, w1, i1, w2, i2);
  const auto fits = [&](int volume) {
    return lattice < static_cast<unsigned int>(model->GetNumMorphBases()) &&
           volume < model->GetNumMorphLattice(static_cast<int>(lattice));
  };
  if (fits(i1)) {
    result = 1;
    model->SetLatticeWeight(static_cast<int>(lattice), i1, w1);
  }
  if (i2 > -1 && fits(i2)) {
    result = 1;
    model->SetLatticeWeight(static_cast<int>(lattice), i2, w2);
  }
  for (int v = 0; v < 3; ++v)
    if (v != i1 && v != i2 && fits(v)) model->SetLatticeWeight(static_cast<int>(lattice), v, 0.0f);
  return result;
}

// 800583AC
// A face slider (0..3, 1 = none): target 0 towards 0, target 1 towards 2, then 1 into 2 up to 3.
void MorphUtilities::GetFacialMorphTargetIndexAndValue(float value, float& w1, int& i1, float& w2, int& i2) {
  i1 = -1;
  w1 = 0.0f;
  i2 = -1;
  w2 = 0.0f;
  if (value >= 0.0f && value < 1.0f) {
    i1 = 0;
    w1 = 1.0f - value;
  } else if (value >= 1.0f && value < 2.0f) {
    i1 = 1;
    w1 = value - 1.0f;
  } else if (value >= 2.0f && value <= 3.0f) {
    i1 = 1;
    w1 = 3.0f - value;
    i2 = 2;
    w2 = value - 2.0f;
  }
}

// 80058474
// A body slider (0..2, 1 = none): volume 1 towards 0, volume 0 towards 2.
void MorphUtilities::GetBodyLatticeIndexAndValue(float value, float& w1, int& i1, float& w2, int& i2) {
  i1 = 0;
  i2 = -1;
  w1 = 0.0f;
  w2 = 0.0f;
  if (value >= 0.0f && value < 1.0f) {
    i1 = 1;
    w1 = 1.0f - value;
  } else if (value >= 1.0f && value <= 2.0f) {
    i1 = 0;
    w1 = value - 1.0f;
  }
}

// 800584F8
// The head shape (0..7): the two volumes around the value, the value 3 (the base head) having none,
// so the volumes above it are numbered one lower.
void MorphUtilities::GetHeadLatticeIndexAndValue(float value, float& w1, int& i1, float& w2, int& i2) {
  i1 = -1;
  i2 = -1;
  w1 = 0.0f;
  w2 = 0.0f;
  if (!(0.0f <= value && value <= 7.0f)) return;
  i1 = static_cast<int>(std::floor(value));
  w1 = 1.0f - (value - std::floor(value));
  if (i1 < 7) {
    i2 = i1 + 1;
    w2 = 1.0f - w1;
  }
  if (i1 == 3) i1 = -1;
  else if (i1 > 3) i1 = i1 - 1;
  if (i2 == 3) i2 = -1;
  else if (i2 >= 4) i2 = i2 - 1;
}
