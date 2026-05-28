#include <game/agent.h>
#include <game/game.h>
#include <giocamo/menu.h>
#include <online/agents.h>
#include <online/setup.h>
#include <tabletop/config.h>
#include <tabletop/game_state.h>
#include <tabletop/input.h>
#include <tabletop/input_recorder.h>
#include <tabletop/models.h>
#include <tabletop/rendering.h>
#include <tabletop/ui.h>
#include <tressette/ai.h>
#include <tressette/gameplay.h>
#include <tressette/models.h>
#include <tressette/neural_agent.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "agent_ui.h"
#include "ui.h"

static Table_State init_table_state(
  tressette::Game_State& state,
  UI_State&              ui_state,
  int                    bottom_player,
  bool                   show_opponent_hand
) {
  auto table                 = Table_State();
  table.is_drop_allowed = [](int, int, int) { return false; };

  // One Thing per card; ids 0..39 match all_cards indices.
  for (const auto& c : state.all_cards) {
    auto t = Thing();
    t.id   = c.id;
    if (c.suit == tressette::Suit::COPPE) t.color = {50, 100, 50, 255};
    if (c.suit == tressette::Suit::DENARI) t.color = {150, 120, 20, 255};
    if (c.suit == tressette::Suit::BASTONI) t.color = {80, 50, 50, 255};
    if (c.suit == tressette::Suit::SPADE) t.color = {70, 80, 150, 255};

    t.image_path = "tressette_card";  // Non-existent path → white bg fallback.
    table.things.push_back(t);
    table.draw_callbacks[c.id] = make_card_draw_callback(state, ui_state, c.id);
  }

  // 6 stack Things appended after cards.
  std::vector<Thing> stacks =
    make_tressette_stacks(bottom_player, show_opponent_hand);
  auto stack_ids = std::vector<int>();
  for (Thing& s : stacks) {
    s.id = (int)table.things.size();
    stack_ids.push_back(s.id);
    table.things.push_back(std::move(s));
  }

  // Populate stack children from game state.
  int base                                       = (int)state.all_cards.size();
  table.things[base + TRESSETTE_HAND_0].children = state.players[0].hand;
  table.things[base + TRESSETTE_HAND_1].children = state.players[1].hand;
  table.things[base + TRESSETTE_TRICKS_0].children =
    state.players[0].tricks_won;
  table.things[base + TRESSETTE_TRICKS_1].children =
    state.players[1].tricks_won;
  table.things[base + TRESSETTE_STOCK_IDX].children = state.stock;
  table.things[base + TRESSETTE_TABLE_IDX].children = state.trick;

  // Root: owns all stacks as direct children.
  auto root = Thing();
  root.name = "root";
  // Root is centered on the screen, so its bounding rect spans (0,0)-(W,H)
  // in world coords.
  root.size        = {(float)tt::WINDOW_WIDTH, (float)tt::WINDOW_HEIGHT};
  root.transform.x = (float)tt::WINDOW_WIDTH / 2.0f;
  root.transform.y = (float)tt::WINDOW_HEIGHT / 2.0f;
  root.id   = (int)table.things.size();
  root.children = stack_ids;
  table.things.push_back(root);
  table.root = root.id;

  for (int sid : stack_ids) {
    update_children_positions(sid, table, false);
  }
  return table;
}

static void update_stacks(Table_State& table, tressette::Game_State& state) {
  int  base    = (int)state.all_cards.size();
  auto refresh = [&](int idx, const std::vector<int>& cards) {
    int sid                    = base + idx;
    table.things[sid].children = cards;
    update_children_positions(sid, table, false);
  };
  refresh(TRESSETTE_HAND_0, state.players[0].hand);
  refresh(TRESSETTE_HAND_1, state.players[1].hand);
  refresh(TRESSETTE_TRICKS_0, state.players[0].tricks_won);
  refresh(TRESSETTE_TRICKS_1, state.players[1].tricks_won);
  refresh(TRESSETTE_STOCK_IDX, state.stock);
  refresh(TRESSETTE_TABLE_IDX, state.trick);
}

// The opponent agent picked for solo (vs-AI) play. Hides the TORCH_AVAILABLE
// fork so the caller doesn't have to know about it.
#include <game/mcts.h>
static Agent* make_ai_opponent() {
#ifdef TORCH_AVAILABLE
  return new tressette::Agent_Minimax_Neural(
    "tressette/tressette_value_traced.pt", 3, 20
  );
#else
  // return new tressette::Tressette_Agent(11, 10);
  return new Agent_MCTS_Stochastic<tressette::Game_State>(
    /* num_iterations       */ 100000,
    /* rollout_depth        */ 40,
    /* num_samples          */ 20,
    /* exploration_constant */ 1.41421356f,
    /* time_budget_seconds  */ 5.0f
  );

#endif
}

// Build the duel for the chosen mode. Mirrors gods_app::make_agent — UI agent
// is always the local seat; the opponent + duel wrapping depend on whether
// `online` is set (peer play), or whether we're in vs-AI vs hot-seat.
static Agent* make_agent(
  Tressette_Agent_UI* agent_ui,
  bool                vs_ai,
  const Online*       online,
  int                 player_index
) {
  if (online) {
    return make_online_duel(agent_ui, *online, player_index);
  }
  Agent* opponent = vs_ai ? make_ai_opponent() : (Agent*)agent_ui;  // hot-seat.
  return new Agent_Duel(agent_ui, opponent, /*swap=*/player_index != 0);
}

static void play_tressette(
  tressette::Game_State& state,
  Table_State&           table,
  UI_State&              ui_state,
  Agent&                 agent,
  int                    bottom_player
) {
  if (!IsWindowReady()) {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "Tressette");
    SetTargetFPS(tt::TARGET_FPS);
  }

  std::optional<Choice> current_choice;

  // Sync stacks only when the game state actually changes (play_card fires
  // this). Calling update_stacks every frame would overwrite children managed
  // by the drag system while a card is mid-drag, causing cards to appear in two
  // stacks.
  // state.on_cards_changed = [&]() { update_stacks(table, state); };

  table.draw_callbacks[-1] =
    [&, bottom_player](const Table_State&, const Input&, bool) {
      for (int i = 0; i < 2; ++i) {
        int  score      = tressette::compute_player_score(state, i);
        bool is_current = (i == state.current_player);
        int  hud_y      = (i == bottom_player) ? (tt::WINDOW_HEIGHT - 56) : 16;
        draw_tressette_player_hud(i, score, is_current, hud_y);
      }
    };

  while (!WindowShouldClose()) {
    if (state.game_over) break;

    Input input = capture_input();
    process_input(table, input);

    BeginDrawing();
    draw_background(input, 0.0f);
    draw_table(table, input);

    current_choice = game_frame(state, agent, current_choice);
    if (current_choice == std::nullopt) {
      update_stacks(table, state);
    }
    EndDrawing();
  }

  if (state.game_over) {
    std::vector<int> scores = {
      tressette::compute_player_score(state, 0),
      tressette::compute_player_score(state, 1),
    };
    draw_tressette_game_over_screen(table, scores);
  }

  CloseWindow();
}

int main(int argc, char** argv) {
  bool vs_ai     = true;
  bool skip_menu = false;
  auto seed      = std::optional<int>();
  for (int i = 1; i < argc; ++i) {
    auto a = std::string(argv[i]);
    if (a == "--hot-seat") {
      vs_ai     = false;
      skip_menu = true;
    } else if (a == "--skip-menu") {
      skip_menu = true;
    } else if (a.rfind("--seed=", 0) == 0) {
      seed = std::atoi(a.c_str() + 7);
    }
  }

  // Local-testing shortcut: `--local-host` / `--local-join` on the command
  // line bypasses the menu and STUN/ntfy entirely. All parsing lives inside
  // online_lib so main doesn't have to know about those flags.
  auto local_conn = setup_local_from_argv(argc, argv);

  // Menu opens its own window; play_tressette reuses it (its InitWindow guard
  // skips when IsWindowReady() returns true).
  Input_Feed inputs;
  init_input_recorder(inputs, Input_Mode::Live, "");
  Menu_Result menu_result;
  if (local_conn) {
    menu_result.mode         = Menu_Result::ONLINE;
    menu_result.online       = local_conn->online;
    menu_result.player_index = local_conn->player_index;
    menu_result.seed         = local_conn->seed;
  } else if (!skip_menu) {
    menu_result =
      run_menu("Tressette", tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, inputs);
  }

  const bool    is_online    = (menu_result.mode == Menu_Result::ONLINE);
  const Online* online       = is_online ? &menu_result.online : nullptr;
  const int     player_index = is_online ? menu_result.player_index : 0;

  // Online peers must deal the same hands — use the seed delivered by the
  // matchmaker. CLI --seed wins for solo play.
  if (is_online) seed = menu_result.seed;

  tressette::Game_State state = tressette::quick_setup(seed);
  auto ui_state               = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);
  // The local player always sits at the bottom of the screen; in online play
  // that may be seat 1, in solo it's always seat 0. Show the opponent's hand
  // only in hot-seat (where one screen is shared).
  const int   bottom_player      = player_index;
  const bool  show_opponent_hand = !vs_ai && !is_online;
  Table_State table =
    init_table_state(state, ui_state, bottom_player, show_opponent_hand);

  auto agent_ui = Tressette_Agent_UI(
    &table, &ui_state, player_index, (int)state.all_cards.size()
  );

  Agent* agent = make_agent(&agent_ui, vs_ai, online, player_index);

  play_tressette(state, table, ui_state, *agent, bottom_player);

  return 0;
}
