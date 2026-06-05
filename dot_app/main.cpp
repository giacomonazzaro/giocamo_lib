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

#include "agent_ui.h"
#include "ui.h"

// Copy the game state into the table: each stack owns the matching cards, the
// play area is cleared (it isn't backed by game state), and the shared pool
// stays face-down until both players have committed their three cards.
static void update_table_from_game(Table_State& table, dot::Game_State& state) {
  int  base      = (int)state.all_cards.size();
  auto set_stack = [&](int stack, const std::vector<int>& cards) {
    table.things[base + stack].children = cards;
    update_children_positions(base + stack, table, false);
  };
  set_stack(DOT_POOL_0, state.players[0].pool);
  set_stack(DOT_POOL_1, state.players[1].pool);
  set_stack(DOT_SHARED, state.shared_pool);
  set_stack(DOT_PLAY_AREA, {});
  set_stack(DOT_HAND_0, state.players[0].hand);
  set_stack(DOT_HAND_1, state.players[1].hand);
  set_stack(DOT_DRAW_0, state.players[0].draw_deck);
  set_stack(DOT_STAR_0, state.players[0].star_deck);
  set_stack(DOT_DRAW_1, state.players[1].draw_deck);
  set_stack(DOT_STAR_1, state.players[1].star_deck);

  // Simulate simultaneous play: until both players have committed, every card
  // played this round stays face-down -- both the shared pool and the cards
  // each player just put in front of them. Cards carried from earlier rounds
  // stay visible, and everything is revealed once both have committed.
  for (const dot::Card& card : state.all_cards) table.things[card.id].face_up = true;
  bool round_revealed = (int)state.shared_pool.size() >= 2 * dot::SHARED_COUNT;
  if (!round_revealed) {
    for (int id : state.shared_pool) table.things[id].face_up = false;
    for (const dot::Player& player : state.players) {
      for (int i = player.revealed_pool_count; i < (int)player.pool.size(); ++i) {
        table.things[player.pool[i]].face_up = false;
      }
    }
  }
}

static Table_State init_table_state(
  dot::Game_State& state,
  UI_State&        ui_state,
  int              bottom_player,
  bool             show_opponent_hand
) {
  Table_State table;
  table.is_drop_allowed = [](int, int, int) { return false; };

  // One Thing per card; ids match all_cards indices. Cream face so the
  // colored dots stand out.
  for (const dot::Card& card : state.all_cards) {
    Thing thing = make_card(card.id);
    thing.color = {235, 225, 205, 255};
    table.things.push_back(thing);
    table.draw_callbacks[card.id] =
      make_dot_card_draw_callback(state.all_cards, ui_state, card.id);
  }

  // Stacks appended after the cards, laid out for the local player's seat.
  std::vector<Thing> stacks =
    make_dot_stacks(bottom_player, show_opponent_hand);
  std::vector<int> stack_ids;
  for (Thing& stack : stacks) {
    stack.id = (int)table.things.size();
    stack_ids.push_back(stack.id);
    table.things.push_back(std::move(stack));
  }

  // Empty texture path: the table is drawn with root.color (a dark surface).
  auto root = create_table_root(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "");
  root.id       = (int)table.things.size();
  root.children = stack_ids;
  root.color    = {15, 15, 15, 255};
  table.things.push_back(root);
  table.root = root.id;

  update_table_from_game(table, state);
  return table;
}

static Agent* make_ai_opponent() {
// The computer opponent for solo play picks its split/discard at random.
#if defined(__EMSCRIPTEN__)
  // Web: the search runs one determinization per frame so the page stays
  // responsive (see the Emscripten branch of Agent_MCTS_Stochastic). Keep
  // num_iterations modest — it bounds each frame's tree (and its allocation) —
  // and gather enough samples to vote well.
  return new Agent_MCTS_Stochastic<dot::Game_State>(
    /* num_iterations       */ 20000,
    /* rollout_depth        */ 40,
    /* num_samples          */ 40,
    /* exploration_constant */ 1.41421356f,
    /* time_budget_seconds  */ 0.0f
  );
#else
  return new Agent_MCTS_Stochastic<dot::Game_State>(
    /* num_iterations       */ 1000000,
    /* rollout_depth        */ 40,
    /* num_samples          */ 50,
    /* exploration_constant */ 1.41421356f,
    /* time_budget_seconds  */ 5.0f
  );
#endif
}

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  // run_menu handles --local-host / --local-join, the skip-menu fallback, and
  // the Play-vs-AI / Play-Online menu, folding the CLI seed in for solo play.
  auto inputs      = Input_Feed(Input_Mode::Live, "");
  auto menu_result = run_menu(
    "D.O.T",
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    inputs,
    argc,
    argv,
    options.skip_menu,
    options.seed
  );

  auto state    = dot::quick_setup(menu_result.seed);
  auto ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);

  // The local player sits at the bottom; in online play that may be seat 1.
  // Show the opponent's hand only in hot-seat (one shared screen).
  const int  bottom_player = menu_result.player_index;
  const bool hot_seat      = !options.vs_ai && !menu_result.is_online();
  auto       table = init_table_state(state, ui_state, bottom_player, hot_seat);

  // The acknowledge pause is owned by the local human. Online play must agree
  // on the owner across both screens, so the host (seat 0) owns it there.
  state.human_player = menu_result.is_online() ? 0 : bottom_player;

  // Dragging is allowed only for the seat that is acting, and only when this
  // screen controls that seat (both seats in hot-seat). Cards move between
  // the acting player's hand and the play area for the split, and between the
  // opponent's pool and the play area for the discard.
  int base = (int)state.all_cards.size();
  table.is_drop_allowed =
    [&state, base, bottom_player, hot_seat](int src, int dst, int) {
      if (src == dst) return true;
      int seat = state.acting_player;
      if (!hot_seat && seat != bottom_player) return false;
      int play_area = base + DOT_PLAY_AREA;
      if (state.phase == dot::Phase::SPLIT) {
        int hand = base + (seat == 0 ? DOT_HAND_0 : DOT_HAND_1);
        return (src == hand && dst == play_area) ||
               (src == play_area && dst == hand);
      }
      if (state.phase == dot::Phase::DISCARD) {
        int pool = base + (seat == 0 ? DOT_POOL_1 : DOT_POOL_0);
        return (src == pool && dst == play_area) ||
               (src == play_area && dst == pool);
      }
      return false;  // Acknowledge pause: nothing is draggable.
    };

  // Always-on HUD overlay (round, tokens, pool totals).
  table.draw_callbacks[-1] =
    [&, bottom_player](const Table_State&, const Input&, bool) {
      draw_dot_hud(state, bottom_player);
    };

  auto agent_ui =
    Dot_Agent_UI(&table, &ui_state, menu_result.player_index, base);
  Agent* agent =
    make_agent_pair(&agent_ui, make_ai_opponent(), menu_result, options.vs_ai);

  play_game(
    state,
    table,
    ui_state,
    *agent,
    inputs,
    menu_result,
    "D.O.T",
    [&] { update_table_from_game(table, state); },
    [&] {
      return std::vector<int>{
        dot::compute_player_score(state, 0),
        dot::compute_player_score(state, 1),
      };
    }
  );
  return 0;
}
