#include "agent_ui.h"

#include <scopa/gameplay.h>
#include <tabletop/config.h>
#include <tabletop/tabletop.h>
#include <tabletop/rendering.h>

#include <set>
#include <string>
#include <variant>

#include "ui.h"

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

// Reset the cross-frame "pick a capture subset" state.
static void clear_pending(Scopa_Agent_UI& agent_ui) {
  agent_ui.pending_played_card_id = -1;
  agent_ui.pending_action_indices.clear();
  agent_ui.pending_capture_options.clear();
}

// Render the floating capture-choice picker over the table. Each entry is a
// row of mini-cards plus a button label; clicking it commits the matching
// action. Returns the chosen action index, or -1 if no choice was made this
// frame.
static int draw_capture_picker(
  const Scopa_Agent_UI&    agent_ui,
  const scopa::Game_State& state,
  const Input&             input
) {
  const int   number_of_options = (int)agent_ui.pending_capture_options.size();
  const float row_height        = 56.0f;
  const float padding           = 12.0f;
  const float panel_width       = 360.0f;
  const float panel_height =
    padding * 2.0f + row_height * (float)number_of_options + 36.0f;
  const float panel_x = (tt::WINDOW_WIDTH - panel_width) * 0.5f;
  const float panel_y = (tt::WINDOW_HEIGHT - panel_height) * 0.5f - 80.0f;

  DrawRectangleRounded(
    Rectangle{panel_x, panel_y, panel_width, panel_height},
    0.12f,
    8,
    Color{20, 20, 30, 230}
  );
  render_text(
    "Choose capture",
    panel_x + padding,
    panel_y + padding,
    24,
    Color{255, 255, 255, 255}
  );

  int chosen = -1;
  for (int i = 0; i < number_of_options; ++i) {
    std::string label;
    const auto& subset = agent_ui.pending_capture_options[i];
    for (int j = 0; j < (int)subset.size(); ++j) {
      if (j > 0) label += " + ";
      label += std::to_string(state.all_cards[subset[j]].rank);
    }
    Rectangle button_rect = {
      panel_x + padding,
      panel_y + padding + 36.0f + row_height * (float)i,
      panel_width - padding * 2.0f,
      row_height - 8.0f,
    };
    if (immediate_button(button_rect, label, input)) {
      chosen = agent_ui.pending_action_indices[i];
    }
  }
  return chosen;
}

int Scopa_Agent_UI::choose_action(Game& game, const Choice& choice) {
  auto&        state   = static_cast<scopa::Game_State&>(game);
  const auto   actions = scopa::enumerate_actions(state);
  auto         options = choice.actions(game);
  auto*        option  = std::get_if<Choose_Option>(&options);
  if (!option || option->targets.empty()) return -1;

  const int base       = stacks_offset;
  const int hand_id    = base + (choice.player_index == 0 ? SCOPA_HAND_0
                                                          : SCOPA_HAND_1);
  const int table_id   = base + SCOPA_TABLE_IDX;

  // Cards in the hand that the rules let the player play. Every card in
  // hand is legal in Scopa — the action set just varies per card.
  std::set<int> playable_hand;
  for (const scopa::Action& action : actions) {
    playable_hand.insert(action.played_card_id);
  }

  // Restrict drops: only from the local hand to the table, and only while
  // we're not already mid-pick. Once a card has been played-but-not-resolved
  // (waiting on capture choice) the only thing allowed is clicking a button.
  Scopa_Agent_UI* self = this;
  table_state->is_drop_allowed =
    [hand_id, table_id, playable_hand, self, &state](int src, int dst, int cid) {
      if (src == dst) return true;
      if (self->pending_played_card_id != -1) return false;
      return state.current_player == self->player_index && src == hand_id &&
             dst == table_id && playable_hand.count(cid) > 0;
    };

  // While picking, highlight the table cards that belong to at least one of
  // the remaining capture options.
  ui_state->highlighted_things.clear();
  if (pending_played_card_id == -1) {
    for (int card_id : playable_hand) ui_state->highlighted_things[card_id] = card_id;
  } else {
    for (const auto& subset : pending_capture_options) {
      for (int card_id : subset) ui_state->highlighted_things[card_id] = card_id;
    }
    ui_state->highlighted_things[pending_played_card_id] = pending_played_card_id;
  }

  // Capture picker has priority: if we're already mid-pick, render it and
  // wait for the click.
  if (pending_played_card_id != -1) {
    int chosen_action = draw_capture_picker(*this, state, *ui_state->input);
    if (chosen_action == -1) return -1;
    clear_pending(*this);
    ui_state->highlighted_things.clear();
    return chosen_action;
  }

  // Otherwise watch for a drop from the hand to the table.
  auto dropped = table_state->poll_dropped_thing();
  if (!dropped) return -1;

  auto [src, dst, dropped_id] = *dropped;
  if (src != hand_id || dst != table_id) return -1;

  // Collect every action that plays the dropped card.
  std::vector<int>              matching_indices;
  std::vector<std::vector<int>> matching_captures;
  for (int i = 0; i < (int)actions.size(); ++i) {
    if (actions[i].played_card_id == dropped_id) {
      matching_indices.push_back(i);
      matching_captures.push_back(actions[i].captured_card_ids);
    }
  }
  if (matching_indices.empty()) return -1;

  if (matching_indices.size() == 1) {
    ui_state->highlighted_things.clear();
    return matching_indices[0];
  }

  // Multiple capture choices: stash them and wait for the player to pick.
  pending_played_card_id  = dropped_id;
  pending_action_indices  = std::move(matching_indices);
  pending_capture_options = std::move(matching_captures);
  return -1;
}
