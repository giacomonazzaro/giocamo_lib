#include <game/agent.h>
#include <game/game.h>
#include <game/minimax.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <mindbug/gameplay.h>
#include <mindbug/models.h>
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <iostream>
#include <vector>

#include "agent_ui.h"
#include "ui.h"

// Mindbug on the table. The table is laid out once here; play_game deals the
// game and drives the loop through these hooks.
struct Mindbug_Giocamo : Giocamo {
  bool show_opponent_hand;

  Mindbug_Giocamo(mindbug::Game_State& game, Mindbug_Agent_UI& agent_ui)
      : Giocamo(game, agent_ui) {}

  void init_table() override {
    auto bottom_player = this->bottom_player;
    auto hot_seat      = this->hot_seat;

    table.is_drop_allowed = [](int, int, int) { return false; };

    // One Thing per card of the deal; ids match the game's card indices. The
    // deal comes later, so a card takes its art in update_table_from_game.
    const int card_count = 2 * (mindbug::HAND_SIZE + mindbug::DRAW_PILE_SIZE);
    for (int card = 0; card < card_count; ++card) {
      table.things.push_back(make_card());
      table.draw_callbacks[card] =
        make_card_draw_callback(this->mindbug_game(), card);
    }

    auto zone_ids = std::vector<int>();
    for (Thing& zone : make_mindbug_stacks(
           bottom_player, tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT
         )) {
      zone_ids.push_back(add_thing(table, std::move(zone)));
    }

    // Empty texture path: the table is drawn with root.color.
    auto root      = create_table_root(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "");
    root._children = zone_ids;
    root.color     = {18, 20, 26, 255};
    table.root     = add_thing(table, std::move(root));

    table.draw_callbacks[-1] = [this](const Table_State&, const Input&, bool) {
      draw_mindbug_hud(this->mindbug_game(), this->bottom_player);
    };
  }

  mindbug::Game_State& mindbug_game() {
    return static_cast<mindbug::Game_State&>(game);
  }

  Mindbug_Agent_UI& mindbug_agent_ui() {
    return static_cast<Mindbug_Agent_UI&>(agent_ui);
  }

  // Every zone owns the cards the game says it holds, and a hand is only face
  // up for the player it belongs to.
  void update_table_from_game() override {
    mindbug::Game_State& state = this->mindbug_game();

    // Clicking a card also starts dragging it, and the card the player just
    // clicked is about to change zone. End the drag first, or the layout would
    // look for it in the zone it has already left.
    table.drag_state = Drag_State();

    for (int card = 0; card < state.all_cards.size(); ++card) {
      const mindbug::Card_Design& design =
        mindbug::card_designs[mindbug::design_of(state, card)];
      table.things[card].image_path = get_image_path(design.image);
    }

    // The choice they belonged to is over. Whoever is asked next puts back the
    // ones it needs.
    clear_highlights(table, state);

    auto set_zone =
      [&](const std::string& name, const std::vector<int>& cards) {
        const int zone               = find_thing(table, name);
        table.things[zone]._children = cards;
        update_children_positions(zone, table, false);
      };

    for (int player = 0; player < 2; ++player) {
      const std::string      prefix = "p" + std::to_string(player) + "_";
      const mindbug::Player& seat   = state.players[player];
      set_zone(prefix + "hand", {seat.hand.begin(), seat.hand.end()});
      set_zone(prefix + "draw", {seat.draw_pile.begin(), seat.draw_pile.end()});
      set_zone(prefix + "discard", {seat.discard.begin(), seat.discard.end()});
      set_zone(
        prefix + "creatures", {seat.creatures.begin(), seat.creatures.end()}
      );

      // You always see your own hand; the opponent's is face down unless both
      // players share this screen.
      const bool visible = player == bottom_player || show_opponent_hand;
      table.things[find_thing(table, prefix + "hand")].face_up = visible;
    }

    // The creature waiting on a Mindbug decision is face up for both players —
    // the opponent has to see what they may take.
    set_zone(
      "played",
      state.played_card == -1 ? std::vector<int>{}
                              : std::vector<int>{state.played_card}
    );
  }

  Agent* agent_opponent() override {
    return new Agent_Minimax_Stochastic<mindbug::Game_State>(
      /* max_depth   */ 13,
      /* num_samples */ 15
    );
  }

  std::vector<int> player_scores() override {
    return {
      mindbug::compute_player_score(this->mindbug_game(), 0),
      mindbug::compute_player_score(this->mindbug_game(), 1),
    };
  }
};

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  if (!mindbug::load_card_designs()) {
    std::cerr << "run mindbug_app from the repository root\n";
    return 1;
  }

  auto game     = mindbug::Game_State();
  auto agent_ui = Mindbug_Agent_UI();
  auto giocamo  = Mindbug_Giocamo(game, agent_ui);

  // Agent* agent = make_agent_pair(
  //   &agent_ui, giocamo.agent_opponent(), menu_result, options.vs_ai
  // );

  play_game(giocamo, options, "Mindbug");
  return 0;
}
