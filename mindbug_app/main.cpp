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

// Copy the game state into the table: every zone owns the cards the game says
// it holds, and a hand is only face up for the player it belongs to.
static void update_table_from_game(
  Table_State&         table,
  mindbug::Game_State& state,
  int                  bottom_player,
  bool                 show_opponent_hand
) {
  // Clicking a card also starts dragging it, and the card the player just
  // clicked is about to change zone. End the drag first, or the layout would
  // look for it in the zone it has already left.
  table.drag_state = Drag_State();

  auto set_zone = [&](const std::string& name, const std::vector<int>& cards) {
    const int zone               = find_thing(table, name);
    table.things[zone]._children = cards;
    update_children_positions(zone, table, false);
  };

  for (int player = 0; player < 2; ++player) {
    const std::string      prefix     = "p" + std::to_string(player) + "_";
    const mindbug::Player& hand_owner = state.players[player];
    set_zone(prefix + "hand", {hand_owner.hand.begin(), hand_owner.hand.end()});
    set_zone(
      prefix + "draw",
      {hand_owner.draw_pile.begin(), hand_owner.draw_pile.end()}
    );
    set_zone(
      prefix + "discard", {hand_owner.discard.begin(), hand_owner.discard.end()}
    );

    std::vector<int> creatures;
    for (int creature : mindbug::creatures_of(state, player)) {
      creatures.push_back(state.creatures[creature].card);
    }
    set_zone(prefix + "creatures", creatures);

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

static Table_State init_table_state(
  mindbug::Game_State& state,
  UI_State&            ui_state,
  int                  bottom_player,
  bool                 show_opponent_hand
) {
  auto table            = Table_State();
  table.is_drop_allowed = [](int, int, int) { return false; };

  // One Thing per dealt card; ids match the game's card indices.
  for (int card = 0; card < state.all_cards.size(); ++card) {
    const mindbug::Card_Design& design =
      mindbug::card_designs[mindbug::design_of(state, card)];
    table.things.push_back(make_card(get_image_path(design.image)));
    table.draw_callbacks[card] = make_card_draw_callback(state, ui_state, card);
  }

  std::vector<int> zone_ids;
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

  update_table_from_game(table, state, bottom_player, show_opponent_hand);
  return table;
}

static Agent* make_ai_opponent() {
  // Mindbug hides the opponent's hand, so the search votes over sampled deals.
  return new Agent_Minimax_Stochastic<mindbug::Game_State>(
    /* max_depth   */ 15,
    /* num_samples */ 30
  );
}

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  if (!mindbug::load_card_designs()) {
    std::cerr << "run mindbug_app from the repository root\n";
    return 1;
  }

  auto inputs      = Input_Feed(Input_Mode::Live, "");
  auto menu_result = run_menu(
    "Mindbug",
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    inputs,
    argc,
    argv,
    options.skip_menu,
    options.seed
  );

  auto state    = mindbug::quick_setup(menu_result.seed);
  auto ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);

  // The local player sits at the bottom; in online play that may be seat 1.
  // Both hands are shown only in hot-seat, where one screen is shared.
  const int  bottom_player = menu_result.player_index;
  const bool hot_seat      = !options.vs_ai && !menu_result.is_online();
  auto       table = init_table_state(state, ui_state, bottom_player, hot_seat);

  table.draw_callbacks[-1] =
    [&state, bottom_player](const Table_State&, const Input&, bool) {
      draw_mindbug_hud(state, bottom_player);
    };

  auto   agent_ui = Mindbug_Agent_UI(&table, &ui_state, bottom_player);
  Agent* agent =
    make_agent_pair(&agent_ui, make_ai_opponent(), menu_result, options.vs_ai);

  play_game(
    state,
    table,
    ui_state,
    *agent,
    inputs,
    menu_result,
    "Mindbug",
    [&] { update_table_from_game(table, state, bottom_player, hot_seat); },
    [&] {
      return std::vector<int>{
        mindbug::compute_player_score(state, 0),
        mindbug::compute_player_score(state, 1),
      };
    }
  );
  return 0;
}
