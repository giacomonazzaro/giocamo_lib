#include <game/agent.h>
#include <game/game.h>
#include <tabletop/config.h>
#include <tabletop/game_state.h>
#include <tabletop/input.h>
#include <tabletop/models.h>
#include <tabletop/rendering.h>
#include <tabletop/ui.h>
#include <tressette/ai.h>
#include <tressette/gameplay.h>
#include <tressette/models.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "agent_ui.h"
#include "neural_agent.h"
#include "ui.h"

static Table_State init_table_state(
  tressette::Game_State& state, UI_State& ui_state, bool both_hands_visible
) {
  auto table = Table_State();

  auto draw = make_card_draw_callback(state, ui_state);

  // One Thing per card; ids 0..39 match all_cards indices.
  for (const auto& c : state.all_cards) {
    auto t       = Thing();
    t.id         = c.id;
    t.image_path = "tressette_card";  // Non-existent path → white bg fallback.
    t.draw_callback = draw;
    table.things.push_back(t);
  }
  table.num_cards = (int)table.things.size();

  // 6 stack Things appended after cards.
  std::vector<Thing> stacks    = make_tressette_stacks(both_hands_visible);
  auto               stack_ids = std::vector<int>();
  for (Thing& s : stacks) {
    s.id = (int)table.things.size();
    stack_ids.push_back(s.id);
    table.things.push_back(std::move(s));
  }

  // Populate stack children from game state.
  int base                                       = table.num_cards;
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
  root.rect = {0.0f, 0.0f, (float)tt::WINDOW_WIDTH, (float)tt::WINDOW_HEIGHT};
  root.id   = (int)table.things.size();
  root.children = stack_ids;
  table.things.push_back(root);
  table.root = root.id;

  for (int sid : stack_ids) {
    update_card_positions(sid, table, false);
  }
  return table;
}

static void update_stacks(Table_State& table, tressette::Game_State& state) {
  int  base    = table.num_cards;
  auto refresh = [&](int idx, const std::vector<int>& cards) {
    int sid                    = base + idx;
    table.things[sid].children = cards;
    update_card_positions(sid, table, false);
  };
  refresh(TRESSETTE_HAND_0, state.players[0].hand);
  refresh(TRESSETTE_HAND_1, state.players[1].hand);
  refresh(TRESSETTE_TRICKS_0, state.players[0].tricks_won);
  refresh(TRESSETTE_TRICKS_1, state.players[1].tricks_won);
  refresh(TRESSETTE_STOCK_IDX, state.stock);
  refresh(TRESSETTE_TABLE_IDX, state.trick);
}

static void play_tressette(
  tressette::Game_State& state,
  Table_State&           table,
  UI_State&              ui_state,
  Agent&                 agent
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
  state.on_cards_changed = [&]() { update_stacks(table, state); };

  table.draw_callback = [&](Table_State*) {
    for (int i = 0; i < 2; ++i) {
      int  score      = tressette::compute_player_score(state, i);
      bool is_current = (i == state.current_player);
      int  hud_y      = (i == 0) ? (tt::WINDOW_HEIGHT - 56) : 16;
      draw_tressette_player_hud(i, score, is_current, hud_y);
    }
  };

  while (!WindowShouldClose()) {
    if (state.game_over) break;

    update_input(table);

    BeginDrawing();
    draw_background(0.0f);
    draw_table(table);

    current_choice = game_frame(state, agent, current_choice);

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
  bool vs_ai = true;
  auto seed  = std::optional<int>();
  for (int i = 1; i < argc; ++i) {
    auto a = std::string(argv[i]);
    if (a == "--hot-seat")
      vs_ai = false;
    else if (a.rfind("--seed=", 0) == 0)
      seed = std::atoi(a.c_str() + 7);
  }

  tressette::Game_State state    = tressette::quick_setup(seed);
  auto                  ui_state = UI_State();
  Table_State           table    = init_table_state(state, ui_state, !vs_ai);

  auto agent_ui = Tressette_Agent_UI(&table, &ui_state);

  Agent* agent_opponent = nullptr;

  if (vs_ai) {
#ifdef TORCH_AVAILABLE
    agent_opponent = new tressette::Agent_Minimax_Neural(
      "tressette/tressette_value_traced.pt", 3, 20
    );
#else
    agent_opponent = new tressette::Tressette_Agent(11, 10);
#endif
  } else {
    agent_opponent = &agent_ui;  // hot-seat.
  }

  auto duel = Agent_Duel(&agent_ui, agent_opponent, /*swap=*/false);
  play_tressette(state, table, ui_state, duel);

  return 0;
}
