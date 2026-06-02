#include "play.h"

#include <raylib.h>

#include <online/agents.h>
#include <online/protocol.h>
#include <online/setup.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>

#include <cstdlib>
#include <string>

void update_zoomed_thing(Table_State& table_state, const Input& input) {
  if (key_down(input, KEY_SPACE)) {
    auto path = find_thing_at(
      (float)input.mouse_x, (float)input.mouse_y, table_state
    );
    table_state.zoomed_thing_id = std::move(path);
  } else {
    table_state.zoomed_thing_id.clear();
  }
}

nlohmann::json serialize_stacks(const Table_State& table_state) {
  nlohmann::json out = nlohmann::json::array();
  for (const Thing& t : table_state.things) {
    out.push_back(t.children);
  }
  return out;
}

void apply_stacks_message(
  Table_State& table_state, const nlohmann::json& arr
) {
  for (size_t i = 0; i < arr.size() && i < table_state.things.size(); ++i) {
    table_state.things[i].children = arr[i].get<std::vector<int>>();
    update_children_positions((int)i, table_state, false);
  }
}

void send_stacks(const Online& online, const Table_State& table_state) {
  nlohmann::json msg;
  msg["type"]   = "stacks";
  msg["stacks"] = serialize_stacks(table_state);
  send_message(online, msg);
}

Menu_Result resolve_play_mode(
  const std::string& title,
  int                window_width,
  int                window_height,
  Input_Feed&        inputs,
  int                argc,
  char**             argv,
  bool               skip_menu
) {
  // --local-host / --local-join bypass the menu and STUN/ntfy entirely.
  if (auto local_conn = setup_local_from_argv(argc, argv)) {
    Menu_Result r;
    r.mode         = Menu_Result::ONLINE;
    r.online       = local_conn->online;
    r.player_index = local_conn->player_index;
    r.seed         = local_conn->seed;
    return r;
  }
  if (skip_menu) return Menu_Result{};
  return run_menu(title, window_width, window_height, inputs);
}

Agent* make_duel(
  Agent* local_agent, Agent* opponent, const Menu_Result& menu_result
) {
  if (menu_result.mode == Menu_Result::ONLINE) {
    return make_online_duel(
      local_agent, menu_result.online, menu_result.player_index
    );
  }
  return new Agent_Duel(
    local_agent, opponent, /*swap=*/menu_result.player_index != 0
  );
}

void draw_game_over_screen(
  Table_State&            table_state,
  const std::string&      result_text,
  const std::vector<int>& scores
) {
  const int   W          = tt::WINDOW_WIDTH;
  const int   H          = tt::WINDOW_HEIGHT;
  const char* title      = "GAME OVER";
  std::string score_line = std::to_string(scores[0]) + " - " +
                           std::to_string(scores[1]);

  while (!WindowShouldClose()) {
    Input input = capture_input();
    BeginDrawing();
    draw_background(input, 0.0f);
    draw_table(table_state, input);
    DrawRectangle(0, 0, W, H, Color{0, 0, 0, 160});
    render_text(
      title,
      (float)(W / 2 - text_width(title, 60) / 2),
      320.0f,
      60,
      Color{255, 255, 255, 255}
    );
    render_text(
      result_text,
      (float)(W / 2 - text_width(result_text, 36) / 2),
      410.0f,
      36,
      Color{255, 215, 0, 255}
    );
    render_text(
      score_line,
      (float)(W / 2 - text_width(score_line, 30) / 2),
      470.0f,
      30,
      Color{200, 200, 200, 255}
    );
    EndDrawing();
  }
}

Play_Options parse_play_args(int argc, char** argv) {
  auto options = Play_Options{};
  for (int i = 1; i < argc; ++i) {
    auto arg = std::string(argv[i]);
    if (arg == "--hot-seat") {
      // Hot-seat = one screen, two humans. No AI, and skip the menu since
      // there's nothing to choose.
      options.vs_ai     = false;
      options.skip_menu = true;
    } else if (arg == "--skip-menu") {
      options.skip_menu = true;
    } else if (arg.rfind("--seed=", 0) == 0) {
      options.seed = std::atoi(arg.c_str() + 7);
    }
  }
  return options;
}

Agent* make_agent_pair(
  Agent*             local_agent,
  Agent*             ai_opponent,
  const Menu_Result& menu_result,
  bool               vs_ai
) {
  // vs-AI: run the search on a worker thread so the main loop stays at 60 FPS.
  // Hot-seat: the local agent plays both seats.
  Agent* opponent = vs_ai ? (Agent*)new Agent_Async(ai_opponent) : local_agent;
  return make_duel(local_agent, opponent, menu_result);
}

void play_game(
  Game&                             state,
  Table_State&                      table,
  Agent&                            agent,
  Input_Feed&                       input_feed,
  const std::string&                window_title,
  std::function<void()>             sync_table,
  std::function<std::vector<int>()> compute_scores
) {
  auto current_choice = std::optional<Choice>();

  // Each frame: ask game_frame for the next move. When it resolves a choice
  // (returns nullopt), let the game-specific code refresh the table from the
  // updated game state.
  auto update = [&](Table_State&, const Input&) {
    current_choice = game_frame(state, agent, current_choice);
    if (current_choice == std::nullopt && sync_table) {
      sync_table();
    }
  };

  run_tabletop(
    table, update, input_feed, tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, window_title
  );

  if (state.is_game_over() && compute_scores) {
    auto scores = compute_scores();
    auto result_text = std::string();
    if (scores[0] > scores[1]) result_text = "Player 1 wins!";
    else if (scores[1] > scores[0]) result_text = "Player 2 wins!";
    else result_text = "It's a tie.";
    draw_game_over_screen(table, result_text, scores);
  }

  CloseWindow();
}
