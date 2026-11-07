// @class Player
// One player of the game (game/player.cpp, two in GameData at +0xF4): its index, the neighbour it
// plays, its pie menu and its live-mode camera.
#pragma once

// 0x10 bytes.
struct Player {
  Player();  // 800594F4
  void Reset(int index);  // 8005950C
  static EVec3& GetInteractorColor();  // 800595AC (s_interactorColor: 1, 1, 3)
  Neighbor* GetPrimaryNeighbor();  // 800595B8
  RET_UNKNOWN GetSelectedNeighbor();  // 8005965C

  int m_index = 0;                     // +0x00
  Neighbor* m_neighbor = nullptr;      // +0x04 the primary neighbour (GetPrimaryNeighbor)
  FlashPiMenu* m_piMenu = nullptr;     // +0x08
  ESimsCam* m_cam = nullptr;           // +0x0C the live-mode camera
};
