// @class MoviePlayerEndgameMovieState after StateMachineState
// The story's ending movie (game/movieplayer.cpp, TheSimsStateMachine state 15): InLevelBaseState's
// "Movie" request (10: the last lot of the story left, cXObjectImpl::TransitionToNewHouse 4) saves and
// unloads the lot and comes here; the movie plays, then the game goes on in live mode.
#pragma once

struct MoviePlayerEndgameMovieState : public StateMachineState {
  explicit MoviePlayerEndgameMovieState(int id = -1);  // 80058820
  void Reset() override;  // 800585FC
  void Update(float dt) override;  // 8005871C
  void GotoGame();  // 80058778
  void Draw(ERC* rc) override;  // 800587E8
  void Shutdown() override;  // 800587EC
  void SetFlowStateCurrent();  // 80058858
};
