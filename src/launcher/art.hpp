// SimmyLauncher: the procedural artwork: sky and clouds, the plumbob, the wordmark, the disc,
// controllers and the co-op layout pictures. Everything is drawn with Canvas; no images are shipped.
#pragma once
#include "canvas.hpp"

namespace launcher {

// The sky, the sun, drifting clouds and the hills; `t` in seconds.
void draw_sky(Canvas& c, float t, float width, float height);

// A faceted, glowing green plumbob centred on (cx, cy), `height` tall, spun by `angle` (radians).
// `glow` 0..1 scales the halo.
void draw_plumbob(Canvas& c, float cx, float cy, float height, float angle, float glow = 1.0f, bool shadow = true);

// "SIMMY" in chunky outlined letters, centred on x, baseline y.
void draw_wordmark(Canvas& c, float x, float y, float alpha = 1.0f);

// A mini disc, spinning shine at `angle`; `present` false draws an empty dashed slot.
void draw_disc(Canvas& c, float cx, float cy, float radius, float angle, bool present, Color hole);

// A friendly controller icon; `connected` false draws a faint outline.
void draw_controller(Canvas& c, float cx, float cy, float width, bool connected, Color accent);

// The co-op layouts: 0 gc, 1 4x3, 2 wide, 3 windows (as pc_settings' split values).
void draw_layout_picture(Canvas& c, Rect area, int split, float highlight);

// Small pictograms for buttons (white, centred on c): play, people, gear, disc, back, power, folder.
enum class Icon { Play, People, Gear, Disc, Back, Power, Folder, Stop, None };
void draw_icon(Canvas& c, Icon icon, Vec2 center, float size, Color col);

}  // namespace launcher
