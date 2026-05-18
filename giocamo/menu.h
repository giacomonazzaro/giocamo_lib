#pragma once

#ifdef __EMSCRIPTEN__
#include <online/online_stub.h>
#else
#include <online/protocol.h>
#include <online/setup.h>
#endif

#include <tabletop/input_recorder.h>

struct Menu_Result {
  enum Mode { VS_AI, ONLINE } mode = VS_AI;
  // ONLINE-only fields, valid when mode == ONLINE.
  int    player_index = 0;
  int    seed         = 0;
  Online online;
};

// Opens a raylib window for the menu and returns the user's choice.
// The window stays open after this returns so main can render the game inside
// it. `recorder` drives the menu's per-frame input — Live captures from raylib,
// Record also stores frames, Playback replays them.
Menu_Result run_menu(
  const char* title, int window_width, int window_height, Input_Feed& recorder
);
