#include "agent_ui.h"

#include <dot/gameplay.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "ui.h"

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

int Dot_Agent_UI::choose_action(Game& game, const Choice& choice) {
  auto&        state = static_cast<dot::Game_State&>(game);
  const Input& input = *ui_state->input;

  // The Commit / Ok button sits in the gap between the hand and the play area.
  Rectangle button = {1010.0f, 862.0f, 170.0f, 56.0f};

  // Acknowledge pause: the shared pool is revealed; wait for the player to
  // look at what the opponent played and press Ok before it is scored.
  if (strcmp(choice.description, "acknowledge") == 0) {
    ui_state->highlighted_things.clear();
    render_text(
      "Cards revealed - press Ok to score", 640.0f, 18.0f, 22,
      Color{255, 235, 150, 255}
    );
    return immediate_button(button, "Ok", input) ? 0 : -1;
  }

  bool split    = (state.phase == dot::Phase::SPLIT);
  int  required = split ? dot::SHARED_COUNT : dot::discard_count(state);

  // The seat that is acting; this is the local player except in hot-seat,
  // where the one agent drives both seats.
  int seat = choice.player_index;

  // The list the chosen action indexes into: the acting seat's hand for the
  // split, the opponent's pool for the discard. The cards dragged into the
  // play area are a subset of this list.
  array<const int> targets = split
                               ? array<const int>(state.players[seat].hand)
                               : array<const int>(state.players[1 - seat].pool);

  int                     play_area_id = stacks_offset + DOT_PLAY_AREA;
  const std::vector<int>& selected = table_state->things[play_area_id].children();

  // Highlight every card the player may drag this turn.
  ui_state->highlighted_things.clear();
  for (int id : targets) ui_state->highlighted_things[id] = id;

  // Instruction at the top center, Commit button between hand and play area.
  std::string instruction = split ? "Drag 3 cards to the play area, then Commit"
                                  : "Drag " + std::to_string(required) +
                                      " opponent cards to discard, then Commit";
  render_text(instruction, 640.0f, 18.0f, 22, Color{255, 235, 150, 255});
  std::string label = "Commit " + std::to_string((int)selected.size()) + "/" +
                      std::to_string(required);
  bool pressed = immediate_button(button, label, input);

  if (!pressed || (int)selected.size() != required) return -1;

  // Turn the selected cards into the matching action index: find each card's
  // position in the target list, then rank that sorted combination.
  std::vector<int> positions;
  for (int card_id : selected) {
    for (int i = 0; i < (int)targets.size(); ++i) {
      if (targets[i] == card_id) {
        positions.push_back(i);
        break;
      }
    }
  }
  std::sort(positions.begin(), positions.end());
  ui_state->highlighted_things.clear();
  return (int)dot::combination_rank((int)targets.size(), positions);
}
