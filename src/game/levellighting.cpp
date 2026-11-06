// Port of the lighting parts of game/e_rlevel.cpp the render effects need (ERLevel::GetSunLight) and
// game/global.cpp's EGlobal::SetBackgroundColor (live mode's sky-coloured clear).
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 80231894
// The outside's ambient colour and the sun (colour, direction) as the light grid last evaluated them.
void ERLevel::GetSunLight(EVec3& ambient, EDirLight& sun) {
  const ELightGrid__RoomData& outside = m_lightGrid.m_rooms[0];
  ambient = outside.m_ambient.m_color;
  sun.m_color = outside.m_sun.m_color;
  sun.m_dir = outside.m_sun.m_dir;
}

// 80041FE4
// The frame's clear colour (its alpha and clear request kept).
void EGlobal::SetBackgroundColor(float r, float g, float b) {
  EVec3 old;
  int alpha = 0;
  _pGfx->GetBackgroundColor(old, alpha);
  EVec3 color{r, g, b};
  _pGfx->SetBackgroundColor(color, alpha, _pGfx->FrameBufferClearRequestFlag() != 0);
}
