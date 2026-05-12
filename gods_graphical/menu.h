#pragma once

#ifdef __EMSCRIPTEN__
#include "online_stub.h"
#else
#include <online/setup.h>
#endif

#include <memory>
#include <string>
#include <utility>

struct Menu_Result {
  enum Mode { VS_AI, ONLINE } mode = VS_AI;
  // ONLINE-only fields, valid when mode == ONLINE.
  int                         player_index = 0;
  int                         seed         = 0;
  std::shared_ptr<UDP_Socket> sock;
  std::pair<std::string, int> friend_addr;
};

// Opens a raylib window for the menu and returns the user's choice.
// The window stays open after this returns so main can render the game inside
// it.
Menu_Result run_menu();
