#include <game/agent.h>
#include <game/game.h>
#include <game/mcts.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <scopa/ai.h>
#include <scopa/gameplay.h>
#include <scopa/models.h>
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "agent_ui.h"
#include "ui.h"

// Scopa on the table. The table is laid out once here; play_game deals the
// game and drives the loop through these hooks.
struct Scopa_Giocamo : Giocamo {
  Scopa_Giocamo(scopa::Game_State& game, Scopa_Agent_UI& agent_ui)
      : Giocamo(game, agent_ui) {}

  scopa::Game_State& scopa_game() {
    return static_cast<scopa::Game_State&>(game);
  }
  const scopa::Game_State& scopa_game() const {
    return static_cast<const scopa::Game_State&>(game);
  }

  Scopa_Agent_UI& scopa_agent_ui() {
    return static_cast<Scopa_Agent_UI&>(agent_ui);
  }

  void init_table() override {
    scopa::Game_State& state    = this->scopa_game();
    Scopa_Agent_UI&    player   = this->scopa_agent_ui();
    player.player_index         = bottom_player;
    table.is_drop_allowed       = [](int, int, int) { return false; };

    // One Thing per card; ids 0..39 match all_cards indices.
    for (const auto& card : state.all_cards) {
      Thing thing = make_card();
      if (card.suit == scopa::Suit::COPPE) thing.color = {50, 100, 50, 255};
      if (card.suit == scopa::Suit::DENARI) thing.color = {150, 120, 20, 255};
      if (card.suit == scopa::Suit::BASTONI) thing.color = {80, 50, 50, 255};
      if (card.suit == scopa::Suit::SPADE) thing.color = {70, 80, 150, 255};
      table.things.push_back(thing);
      table.draw_callbacks[card.id] =
        make_card_draw_callback(state, player.ui_state, card.id);
    }

    // 6 stack Things appended after cards. The opponent's hand is face up only
    // in hot-seat, where one screen is shared.
    std::vector<Thing> stacks = make_scopa_stacks(bottom_player, hot_seat);
    std::vector<int>   stack_ids;
    for (Thing& stack : stacks) {
      stack_ids.push_back(add_thing(table, std::move(stack)));
    }

    // Root: a wooden table surface owning all stacks as direct children.
    auto root = create_table_root(
      tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png"
    );
    root._children = stack_ids;
    table.root     = add_thing(table, std::move(root));

    // Per-player HUD overlay. Drawn on top of every frame via the -1 callback.
    table.draw_callbacks[-1] = [this](const Table_State&, const Input&, bool) {
      const scopa::Game_State& state = this->scopa_game();
      for (int i = 0; i < 2; ++i) {
        bool is_current = (i == state.current_player);
        int  hud_y = (i == this->bottom_player) ? (tt::WINDOW_HEIGHT - 56) : 16;
        draw_scopa_player_hud(state, i, is_current, hud_y);
      }
    };
  }

  void update_table_from_game() override {
    scopa::Game_State& state = this->scopa_game();

    auto refresh = [&](const char* name, const std::vector<int>& cards) {
      int stack_id                     = find_thing(table, name);
      table.things[stack_id]._children = cards;
      update_children_positions(stack_id, table, false);
    };
    refresh("p0_hand", state.players[0].hand);
    refresh("p1_hand", state.players[1].hand);
    refresh("p0_captured", state.players[0].captured);
    refresh("p1_captured", state.players[1].captured);
    refresh("stock", state.stock);
    refresh("table", state.table);
  }

  // Leaving playground: the table is what the player arranged, so read it back
  // into the game. Card Things carry the game's card indices, so a stack's
  // children are the cards of that stack.
  void update_game_from_table() override {
    table.is_drop_allowed    = [](int, int, int) { return false; };
    scopa::Game_State& state = this->scopa_game();

    auto stack_cards = [&](const char* name) -> const std::vector<int>& {
      return table.things[find_thing(table, name)].children();
    };
    state.players[0].hand     = stack_cards("p0_hand");
    state.players[1].hand     = stack_cards("p1_hand");
    state.players[0].captured = stack_cards("p0_captured");
    state.players[1].captured = stack_cards("p1_captured");
    state.stock               = stack_cards("stock");
    state.table               = stack_cards("table");
  }

  Agent* agent_opponent() override {
    return new Agent_MCTS_Stochastic<scopa::Game_State>(
      /* num_samples          */ 20,
      /* num_iterations       */ 100000,
      /* rollout_depth        */ 60,
      /* exploration_constant */ 1.41421356f,
      /* total_time_budget    */ 3.0f
    );
  }

  std::vector<int> player_scores() const override {
    return {
      scopa::compute_player_score(this->scopa_game(), 0),
      scopa::compute_player_score(this->scopa_game(), 1),
    };
  }
};

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  auto game     = scopa::Game_State();
  auto agent_ui = Scopa_Agent_UI();
  auto giocamo  = Scopa_Giocamo(game, agent_ui);

  play_game(giocamo, options, "Scopa Scientifica");
  return 0;
}
