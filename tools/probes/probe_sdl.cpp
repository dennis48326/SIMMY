// Startup probe: links SDL3 only.
#include <SDL3/SDL.h>
#include <cstdio>
int main(int, char**) { std::printf("probe_sdl ok, SDL %d\n", SDL_GetVersion()); return 0; }
