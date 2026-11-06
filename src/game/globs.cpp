// Port of game/globs.cpp (partial): the simulation's globals. Ported: the object database
// (Globs::pObjectFolder), the neighbourhood (Globs::pNeighborhood), the careers (Globs::pCareers). Still stubbed: ingredients, the house object
// and the random seed. The sound player (Globs::pSound) is game/gamesound.cpp.
#include "simmy/types_gen.hpp"
#include "simmy/unported_globals.hpp"

// 800B81F8
void Globs::Startup() {
  SIMMY_STUB("Globs::Startup: lighting parameters", 0x800B81F8);
  Globs_pCareers = Careers::CreateInstance();
  ObjectFolder::CreateInstance();
  Globs_pObjectFolder->Init(const_cast<char*>(""), true);  // GC: DAT_803D9B60, the game's root
  SIMMY_STUB("Globs::Startup: ingredients", 0x800B81F8);
  Neighborhood::CreateInstance();
  SIMMY_STUB("Globs::Startup: House, GlobalDispatch, random seed", 0x800B81F8);
  Globs_pSound = new cSoundPlayer;
  Globs_pSound->Initialize();
}

// 800B82D4
void Globs::Shutdown() {
  if (Globs_pSound) {
    Globs_pSound->Shutdown();
    delete Globs_pSound;
    Globs_pSound = nullptr;
  }
  SIMMY_STUB("Globs::Shutdown: house, NghResFile, careers", 0x800B82D4);
  Neighborhood::DestroyInstance(Globs_pNeighborhood);
  ObjectFolder::DestroyInstance(nullptr);
  if (Globs_pCareers) {
    Careers::DestroyInstance(Globs_pCareers);
    Globs_pCareers = nullptr;
  }
}
