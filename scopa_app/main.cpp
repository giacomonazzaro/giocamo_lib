#include <game/agent.h>
#include <game/game.h>
#include <giocamo/menu.h>
#include <scopa/ai.h>
#include <scopa/gameplay.h>
#include <scopa/models.h>
#include <tabletop/config.h>
#include <tabletop/game_state.h>
#include <tabletop/input.h>
#include <tabletop/input_recorder.h>
#include <tabletop/models.h>
#include <tabletop/rendering.h>
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

static Table_State init_table_state(
  scopa::Game_State& state,
  UI_State&          ui_state,
  int                bottom_player,
  bool               show_opponent_hand
) {
  Table_State table;
  table.is_drop_card_allowed = [](int, int, int) { return false; };

  // One Thing per card; ids 0..39 match all_cards indices.
  for (const auto& card : state.all_cards) {
    Thing thing;
    thing.id = card.id;
    if (card.suit == scopa::Suit::COPPE) thing.color = {50, 100, 50, 255};
    if (card.suit == scopa::Suit::DENARI) thing.color = {150, 120, 20, 255};
    if (card.suit == scopa::Suit::BASTONI) thing.color = {80, 50, 50, 255};
    if (card.suit == scopa::Suit::SPADE) thing.color = {70, 80, 150, 255};
    thing.image_path = "scopa_card";  // Non-existent path -> white bg fallback.
    table.things.push_back(thing);
    table.draw_callbacks[card.id] =
      make_card_draw_callback(state, ui_state, card.id);
  }
  table.num_cards = (int)table.things.size();

  // 6 stack Things appended after cards.
  std::vector<Thing> stacks =
    make_scopa_stacks(bottom_player, show_opponent_hand);
  std::vector<int> stack_ids;
  for (Thing& stack : stacks) {
    stack.id = (int)table.things.size();
    stack_ids.push_back(stack.id);
    table.things.push_back(std::move(stack));
  }

  // Populate stack children from game state.
  int base                                       = table.num_cards;
  table.things[base + SCOPA_HAND_0].children     = state.players[0].hand;
  table.things[base + SCOPA_HAND_1].children     = state.players[1].hand;
  table.things[base + SCOPA_CAPTURED_0].children = state.players[0].captured;
  table.things[base + SCOPA_CAPTURED_1].children = state.players[1].captured;
  table.things[base + SCOPA_STOCK_IDX].children  = state.stock;
  table.things[base + SCOPA_TABLE_IDX].children  = state.table;

  // Root: owns all stacks as direct children.
  Thing root;
  root.name = "root";
  root.rect = {0.0f, 0.0f, (float)tt::WINDOW_WIDTH, (float)tt::WINDOW_HEIGHT};
  root.id   = (int)table.things.size();
  root.children = stack_ids;
  table.things.push_back(root);
  table.root = root.id;

  for (int stack_id : stack_ids) {
    update_card_positions(stack_id, table, false);
  }
  return table;
}

static void update_stacks(Table_State& table, scopa::Game_State& state) {
  int  base    = table.num_cards;
  auto refresh = [&](int idx, const std::vector<int>& cards) {
    int stack_id                    = base + idx;
    table.things[stack_id].children = cards;
    update_card_positions(stack_id, table, false);
  };
  refresh(SCOPA_HAND_0, state.players[0].hand);
  refresh(SCOPA_HAND_1, state.players[1].hand);
  refresh(SCOPA_CAPTURED_0, state.players[0].captured);
  refresh(SCOPA_CAPTURED_1, state.players[1].captured);
  refresh(SCOPA_STOCK_IDX, state.stock);
  refresh(SCOPA_TABLE_IDX, state.table);
}

#include <game/mcts.h>
static Agent* make_ai_opponent() {
  return new Agent_MCTS_Stochastic<scopa::Game_State>(
    /* num_iterations       */ 100000,
    /* rollout_depth        */ 60,
    /* num_samples          */ 20,
    /* exploration_constant */ 1.41421356f,
    /* time_budget_seconds  */ 3.0f
  );
}

static Agent* make_agent(
  Scopa_Agent_UI* agent_ui, bool vs_ai, int player_index
) {
  // Run the AI on a worker thread so the main loop keeps rendering at full
  // FPS while it searches. The UI agent stays on the main thread because it
  // already returns -1 each frame until the player commits.
  Agent* opponent = vs_ai ? (Agent*)new Agent_Async(make_ai_opponent())
                          : (Agent*)agent_ui;  // hot-seat.
  return new Agent_Duel(agent_ui, opponent, /*swap=*/player_index != 0);
}

static void play_scopa(
  scopa::Game_State& state,
  Table_State&       table,
  UI_State&          ui_state,
  Agent&             agent,
  int                bottom_player
) {
  if (!IsWindowReady()) {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "Scopa Scientifica");
    SetTargetFPS(tt::TARGET_FPS);
  }

  std::optional<Choice> current_choice;

  table.draw_callbacks[-1] =
    [&, bottom_player](const Table_State&, const Input&, bool) {
      for (int i = 0; i < 2; ++i) {
        bool is_current = (i == state.current_player);
        int  hud_y      = (i == bottom_player) ? (tt::WINDOW_HEIGHT - 56) : 16;
        draw_scopa_player_hud(state, i, is_current, hud_y);
      }
    };

  while (!WindowShouldClose()) {
    if (state.game_over) break;

    Input input    = capture_input();
    ui_state.input = &input;
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
      scopa::compute_player_score(state, 0),
      scopa::compute_player_score(state, 1),
    };
    draw_scopa_game_over_screen(table, scores);
  }

  CloseWindow();
}

int main(int argc, char** argv) {
  bool vs_ai     = true;
  bool skip_menu = false;
  auto seed      = std::optional<int>();
  for (int i = 1; i < argc; ++i) {
    auto arg = std::string(argv[i]);
    if (arg == "--hot-seat") {
      vs_ai     = false;
      skip_menu = true;
    } else if (arg == "--skip-menu") {
      skip_menu = true;
    } else if (arg.rfind("--seed=", 0) == 0) {
      seed = std::atoi(arg.c_str() + 7);
    }
  }

  Input_Feed inputs;
  init_input_recorder(inputs, Input_Mode::Live, "");
  Menu_Result menu_result;
  if (!skip_menu) {
    menu_result = run_menu(
      "Scopa Scientifica", tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, inputs
    );
  }

  const int         player_index = 0;
  scopa::Game_State state        = scopa::quick_setup(seed);
  auto              ui_state = UI_State(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);
  const int         bottom_player      = player_index;
  const bool        show_opponent_hand = !vs_ai;
  Table_State       table =
    init_table_state(state, ui_state, bottom_player, show_opponent_hand);

  auto   agent_ui = Scopa_Agent_UI(&table, &ui_state, player_index);
  Agent* agent    = make_agent(&agent_ui, vs_ai, player_index);

  play_scopa(state, table, ui_state, *agent, bottom_player);
  return 0;
}
