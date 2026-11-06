// @class THPVideoInfo
// @class THPAudioInfo
// The THP movie component descriptions (Nintendo THP player, as used by engine/e_ngcmovie.cpp).
#pragma once

struct THPVideoInfo {     // 0x0C bytes
  unsigned int xSize = 0;
  unsigned int ySize = 0;
  unsigned int videoType = 0;
};

struct THPAudioInfo {     // 0x10 bytes
  unsigned int sndChannels = 0;
  unsigned int sndFrequency = 0;
  unsigned int sndNumSamples = 0;
  unsigned int sndNumTracks = 0;
};
