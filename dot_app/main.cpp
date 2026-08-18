#include <dot/gameplay.h>
#include <dot/models.h>
#include <game/agent.h>
#include <game/game.h>
#include <game/mcts.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <vector>

#include <struct/imgui.h>  // for draw_editor_ui()
#include <struct/json.h>   // for to_json()

#include "agent_ui.h"
#include "ui.h"

// D.O.T on the table. The table is laid out once here; play_game deals the
// game and drives the loop through these hooks.
struct Dot_Giocamo : Giocamo_With_History<dot::Game_State> {
  Dot_Giocamo(dot::Game_State& game, Dot_Agent_UI& agent_ui)
      : Giocamo_With_History<dot::Game_State>(game, agent_ui) {}

  dot::Game_State& dot_game() { return static_cast<dot::Game_State&>(game); }
  const dot::Game_State& dot_game() const {
    return static_cast<const dot::Game_State&>(game);
  }

  Dot_Agent_UI& dot_agent_ui() { return static_cast<Dot_Agent_UI&>(agent_ui); }

  void init_table() override {
    dot::Game_State& state  = this->dot_game();
    Dot_Agent_UI&    player = this->dot_agent_ui();
    player.player_index     = bottom_player;

    // The acknowledge pause is owned by the local human. Online play must agree
    // on the owner across both screens, so seat 0 owns it there.
    state.human_player = hot_seat ? bottom_player : 0;

    // One Thing per card; ids match all_cards indices. Cream face so the
    // colored dots stand out.
    for (const dot::Card& card : dot::all_cards) {
      Thing thing = make_card();
      thing.color = {235, 225, 205, 255};
      table.things.push_back(thing);
      table.draw_callbacks[card.id] =
        make_dot_card_draw_callback(dot::all_cards, player.ui_state, card.id);
    }

    // Stacks appended after the cards, laid out for the local player's seat.
    // The opponent's hand is face up only in hot-seat, where one screen is
    // shared.
    std::vector<Thing> stacks = make_dot_stacks(bottom_player, hot_seat);
    std::vector<int>   stack_ids;
    for (Thing& stack : stacks) {
      stack_ids.push_back(add_thing(table, std::move(stack)));
    }

    // Empty texture path: the table is drawn with root.color (a dark surface).
    auto root      = create_table_root(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "");
    root._children = stack_ids;
    root.color     = {15, 15, 15, 255};
    table.root     = add_thing(table, std::move(root));

    // Dragging is allowed only for the seat that is acting, and only when this
    // screen controls that seat (both seats in hot-seat). Cards move between
    // the acting player's hand and the play area for the split, and between the
    // opponent's pool and the play area for the discard.
    table.is_drop_allowed = [this](int src, int dst, int) {
      if (src == dst) return true;
      dot::Game_State& state = this->dot_game();
      int              seat  = state.acting_player;
      if (!this->hot_seat && seat != this->bottom_player) return false;
      int play_area = find_thing(table, "play_area");
      if (state.phase == dot::Phase::SPLIT) {
        int hand = find_thing(table, seat == 0 ? "p0_hand" : "p1_hand");
        return (src == hand && dst == play_area) ||
               (src == play_area && dst == hand);
      }
      if (state.phase == dot::Phase::DISCARD) {
        // The discard phase takes from the *opponent's* pool.
        int pool = find_thing(table, seat == 0 ? "p1_pool" : "p0_pool");
        return (src == pool && dst == play_area) ||
               (src == play_area && dst == pool);
      }
      return false;  // Acknowledge pause: nothing is draggable.
    };

    // Always-on HUD overlay (round, tokens, pool totals).
    table.draw_callbacks[-1] = [this](const Table_State&, const Input&, bool) {
      draw_dot_hud(this->dot_game(), this->bottom_player);
    };
  }

  // Copy the game state into the table: each stack owns the matching cards, the
  // play area is cleared (it isn't backed by game state), and the shared pool
  // stays face-down until both players have committed their three cards.
  void update_table_from_game() override {
    dot::Game_State& state = this->dot_game();

    auto set_stack = [&](const char* name, array<const int> cards) {
      int stack_id = find_thing(table, name);
      table.things[stack_id]._children.assign(
        cards.data, cards.data + cards.size()
      );
      update_children_positions(stack_id, table, false);
    };
    set_stack("p0_pool", state.players[0].pool);
    set_stack("p1_pool", state.players[1].pool);
    set_stack("shared", state.shared_pool);
    set_stack("play_area", {});
    set_stack("p0_hand", state.players[0].hand);
    set_stack("p1_hand", state.players[1].hand);
    set_stack("p0_draw", state.players[0].draw_deck);
    set_stack("p0_star", state.players[0].star_deck);
    set_stack("p1_draw", state.players[1].draw_deck);
    set_stack("p1_star", state.players[1].star_deck);

    // Simulate simultaneous play: until both players have committed, every card
    // played this round stays face-down -- both the shared pool and the cards
    // each player just put in front of them. Cards carried from earlier rounds
    // stay visible, and everything is revealed once both have committed.
    for (const dot::Card& card : dot::all_cards)
      table.things[card.id].face_up = true;
    bool round_revealed = (int)state.shared_pool.size() >=
                          2 * dot::SHARED_COUNT;
    if (!round_revealed) {
      for (int id : state.shared_pool) table.things[id].face_up = false;
      for (const dot::Player& player : state.players) {
        for (int i = player.revealed_pool_count; i < (int)player.pool.size();
             ++i) {
          table.things[player.pool[i]].face_up = false;
        }
      }
    }
  }

  // The play area holds the cards the player is about to commit, so it is not
  // backed by game state and the table is never read back into the game.
  void update_game_from_table() override {}

  // The computer opponent for solo play.
  Agent* agent_opponent() override {
#if defined(__EMSCRIPTEN__)
    // Web: the search runs one determinization per frame so the page stays
    // responsive (see the Emscripten branch of Agent_MCTS_Stochastic). Keep
    // num_iterations modest — it bounds each frame's tree (and its allocation)
    // — and gather enough samples to vote well.
    return new Agent_MCTS_Stochastic<dot::Game_State>(
      /* num_samples          */ 40,
      /* num_iterations       */ 20000,
      /* rollout_depth        */ 40,
      /* exploration_constant */ 1.41421356f,
      /* total_time_budget    */ 0.0f
    );
#else
    return new Agent_MCTS_Stochastic<dot::Game_State>(
      /* num_samples          */ 50,
      /* num_iterations       */ 1000000,
      /* rollout_depth        */ 40,
      /* exploration_constant */ 1.41421356f,
      /* total_time_budget    */ 5.0f
    );
#endif
  }

  std::vector<int> player_scores() const override {
    return {
      dot::compute_player_score(this->dot_game(), 0),
      dot::compute_player_score(this->dot_game(), 1),
    };
  }
};

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  auto game     = dot::Game_State();
  auto agent_ui = Dot_Agent_UI();
  auto giocamo  = Dot_Giocamo(game, agent_ui);

  play_game(giocamo, options, "D.O.T");
  return 0;
}
