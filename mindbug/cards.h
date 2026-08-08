#pragma once

#include <mindbug/models.h>

namespace mindbug {

// The three moments a card can act on, each looked up by the creature's
// design. An ability that asks the players something pushes the choices onto
// state.queue instead of resolving right away.
void trigger_play(Game_State& state, int creature_index);
void trigger_attack(Game_State& state, int creature_index);
void trigger_defeated(Game_State& state, int creature_index);

}  // namespace mindbug
