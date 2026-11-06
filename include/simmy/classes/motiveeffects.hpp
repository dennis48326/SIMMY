// @class PiecewiseFn
// @class MotiveEffects after PiecewiseFn
// How a person rates its motives for autonomy (game/motiveeffects.cpp, game/piecewise.cpp,
// game/motivecurve.cpp): nine curves (PiecewiseFn), one per motive the autonomy weighs, map a motive
// value to a score; the current score is their mean, and an interaction's score is the mean after its
// tree table advertisements (TTAB ads) are added to the motives.
#pragma once
#include <vector>

// A curve through points (x ascending), linear between them and flat outside.
struct PiecewiseFn {
  struct Point {
    float x = 0.0f, y = 0.0f;
  };
  PiecewiseFn();  // 8012CBE8
  ~PiecewiseFn();  // 8012CC04
  void Reset();  // 8012CC48
  void SetMaxPoints(int count);  // 8012CC54
  void AddPoint(Point& p);  // 8012CCE4
  void AddPointsFromText(const char* text);  // 8012CE1C
  // PC: the evaluation MotiveEffects inlines (GetCurrentScore / GetInteractionScore).
  float Eval(float v) const;

  std::vector<Point> m_points;   // +0x00 (GC: the points and the slopes' arrays, the count +8, max +0xC)
  std::vector<float> m_slopes;   // +0x04 1 / (x[i+1] - x[i])
  int m_max = 0;                 // +0x0C
};

// GC: cXPersonImpl +0x408 (made in the constructor, its curves loaded by LoadMotiveEffects).
struct MotiveEffects {
  explicit MotiveEffects(cXPerson* person);  // 800C2F34
  // The mean of the curves at the person's motives.
  double GetCurrentScore();  // 800C3058
  // The same with the TTAB entry's ads added (each ad: min, + range x the person's personality trait
  // / 1000 when it names one; thousandths of a motive point).
  double GetInteractionScore(const unsigned char* entry);  // 800C31AC
  // MotiveCurveSet::LoadFromStringSet (800C2E98): one point list per curve.
  void LoadFromStringSet(const char* const* texts, int count);

  struct Curve {
    PiecewiseFn m_fn;
    int m_motive = -1;  // +0x10
  };
  std::vector<Curve> m_curves;  // +0x00 (count +0x04: 9)
  cXPerson* m_person = nullptr;  // +0xBC
  float* m_motives = nullptr;    // +0xC0 the person's motive values
};

// The autonomy tuning (treetab.cpp / person.cpp globals), set by UpdateAutonomyConstants.
extern float gLowAttenuation, gModerateAttenuation, gHighAttenuation;
extern float gVisLowAttenuation, gVisModerateAttenuation, gVisHighAttenuation;
extern float gFunctionalScoreDistanceAttenuation;
// AutonomyConstantsClient::UpdateConstants (8011A9E8): the tuning from the global file's constants.
void UpdateAutonomyConstants();
// FloatConstantsQuickData::Get (800B6DF0) on the global object file's constants group `id`.
float GlobalFloatConstant(int id, const char* name, float def);
