#include "agent_ui.h"

#include <mindbug/gameplay.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>

#include <algorithm>
#include <string>
#include <variant>

#include "ui.h"

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

using namespace mindbug;

// The attacking creature, named so the defender knows what is coming.
static std::string attacker_name(const Game_State& state) {
  if (state.attacker == -1) return "";
  const int design = design_of(state, state.attacker);
  return card_designs[design].name + " (" +
         std::to_string(effective_power(state, state.attacker)) + ")";
}

// What the player is being asked, by the name the game gives the choice.
static std::string instruction(const Game_State& state, const Choice& choice) {
  if (choice.description == "turn")
    return "Play a card from your hand, or attack with a creature";
  if (choice.description == "mindbug")
    return "Use a Mindbug to take this creature?";
  if (choice.description == "block")
    return attacker_name(state) + " is attacking. Block it?";
  if (choice.description == "hunt")
    return attacker_name(state) +
           " is attacking. Choose the blocker, or leave the choice to the "
           "opponent";
  if (choice.description == "frenzy")
    return attacker_name(state) + " survived. Attack a second time?";
  if (choice.description == "defeat") return "Choose a creature to defeat";
  if (choice.description == "take-control")
    return "Choose a creature to take control of";
  if (choice.description == "discard") return "Choose cards to discard";
  if (choice.description == "play-from-discard")
    return "Play a card from a discard pile";
  return std::string(choice.text_description);
}

// The card on the table an action target stands for, or -1 when the target is
// "no creature" — the option to let an attack through.
static int card_of_target(const Choice& choice, int target) {
  if (choice.description == "turn") return unpack_turn_action(target).card;
  return target;
}

// The targets a Choose offers, in option order.
static std::vector<int> targets_of(const Choose& actions) {
  if (auto* single = std::get_if<Choose_Card>(&actions)) {
    return std::vector<int>(single->targets.begin(), single->targets.end());
  }
  const Choose_Cards& multiple = std::get<Choose_Cards>(actions);
  return std::vector<int>(multiple.targets.begin(), multiple.targets.end());
}

static void highlight_card(
  Table_State& table, const mindbug::Game_State& state, int card
) {
  table.draw_callbacks[card] = make_card_draw_callback(state, card, true);
}

static void clear_highlights(
  Table_State& table, const mindbug::Game_State& state
) {
  for (int card = 0; card < state.all_cards.size(); ++card) {
    table.draw_callbacks[card] = make_card_draw_callback(state, card);
  }
}

int Mindbug_Agent_UI::choose_action(Game& game, const Choice& choice) {
  Game_State&  state   = static_cast<Game_State&>(game);
  auto&        table   = this->table;
  const Input& input   = *this->input;
  Choose       actions = choice.actions(game);

  clear_highlights(table, state);
  render_text(
    instruction(state, choice),
    (float)tt::WINDOW_WIDTH / 2.0f - 300.0f,
    16.0f,
    22,
    Color{255, 235, 150, 255}
  );

  // Buttons run down the right-hand side, under the score line.
  Rectangle button = place_on_screen(200, 46, "right", "center", 24);

  // The Mindbug decision is the only choice that isn't about a card.
  if (auto* options = std::get_if<Choose_Option>(&actions)) {
    for (int i = 0; i < (int)options->targets.size(); ++i) {
      if (immediate_button(button, options->targets[i], input)) return i;
      button.y += button.height + 14.0f;
    }
    return -1;
  }

  std::vector<int> targets = targets_of(actions);

  // One target to pick: highlight them all and take the one clicked.
  if (std::holds_alternative<Choose_Card>(actions)) {
    for (int i = 0; i < (int)targets.size(); ++i) {
      const int card = card_of_target(choice, targets[i]);
      if (card == -1) {
        // A hunter leaves the choice to the defender; the defender lets the
        // attack through.
        const char* label = choice.description == "hunt" ? "Opponent chooses"
                                                         : "Don't block";
        if (immediate_button(button, label, input)) {
          clear_highlights(table, state);
          return i;
        }
        continue;
      }
      highlight_card(table, state, card);
      if (thing_pressed(card, table, input)) {
        clear_highlights(table, state);
        return i;
      }
    }
    return -1;
  }

  // Several targets to pick: click to add to the selection, then confirm.
  const Choose_Cards& multiple = std::get<Choose_Cards>(actions);
  for (int target : targets) {
    const int  card   = card_of_target(choice, target);
    const bool picked = std::find(selection.begin(), selection.end(), target) !=
                        selection.end();
    if (!picked) highlight_card(table, state, card);
    if (!picked && (int)selection.size() < multiple.count &&
        thing_pressed(card, table, input)) {
      selection.push_back(target);
    }
  }

  const bool complete = multiple.up_to ||
                        (int)selection.size() == multiple.count ||
                        (int)selection.size() == (int)targets.size();
  if (!complete) return -1;

  const std::string label = "Confirm " + std::to_string((int)selection.size()) +
                            "/" + std::to_string(multiple.count);
  if (!immediate_button(button, label, input)) return -1;

  // Answer with the option holding exactly the picked targets.
  std::vector<std::vector<int>> combinations =
    target_combinations(targets, multiple.count, multiple.up_to);
  std::sort(selection.begin(), selection.end());
  for (int i = 0; i < (int)combinations.size(); ++i) {
    if (combinations[i] != selection) continue;
    selection.clear();
    clear_highlights(table, state);
    return i;
  }
  return -1;
}
