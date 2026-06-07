#include "play.h"

#include <online/agents.h>
#include <online/protocol.h>
#include <online/setup.h>
#include <raylib.h>
#include <tabletop/config.h>
#include <tabletop/rendering.h>
#include <tabletop/ui.h>

#include <cstdlib>
#include <random>
#include <string>

void update_zoomed_thing(Table_State& table_state, const Input& input) {
  if (key_down(input, KEY_SPACE)) {
    auto path =
      find_thing_at((float)input.mouse_x, (float)input.mouse_y, table_state);
    table_state.zoomed_thing_id = std::move(path);
  } else {
    table_state.zoomed_thing_id.clear();
  }
}

nlohmann::json serialize_table_state(const Table_State& table_state) {
  nlohmann::json out = nlohmann::json::array();
  for (const Thing& t : table_state.things) {
    out.push_back(t.children());
  }
  return out;
}

void apply_table_state_message(
  Table_State& table_state, const nlohmann::json& arr
) {
  for (size_t i = 0; i < arr.size() && i < table_state.things.size(); ++i) {
    table_state.things[i]._children = arr[i].get<std::vector<int>>();
    update_children_positions((int)i, table_state, false);
  }
}

void send_table_state(const Online& online, const Table_State& table_state) {
  nlohmann::json msg;
  msg["type"]        = "table_state";
  msg["table_state"] = serialize_table_state(table_state);
  send_message(online, msg);
}

Menu_Result run_menu(
  const std::string& title,
  int                window_width,
  int                window_height,
  Input_Feed&        inputs,
  int                argc,
  char**             argv,
  bool               skip_menu,
  int                cli_seed
) {
  // --local-host / --local-join bypass the menu and STUN/ntfy entirely.
  if (auto local_conn = setup_local_from_argv(argc, argv)) {
    Menu_Result result;
    result.mode         = Menu_Result::ONLINE;
    result.online       = local_conn->online;
    result.player_index = local_conn->player_index;
    result.seed         = local_conn->seed;
    return result;
  }
  if (skip_menu) {
    auto result = Menu_Result{};
    result.seed = cli_seed;  // Solo play; honor --seed=N from the command line.
    return result;
  }
  auto result = run_menu(title, window_width, window_height, inputs);
  if (!result.is_online()) result.seed = cli_seed;
  return result;
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
  auto options    = Play_Options{};
  bool seed_given = false;
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
      seed_given   = true;
    }
  }
  // Make seed always carry a real value so callers never have to think about
  // "is this set?". A fresh random one is picked when --seed isn't passed.
  if (!seed_given) {
    options.seed = (int)std::random_device{}();
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
  Game&                                      state,
  Table_State&                               table,
  UI_State&                                  ui_state,
  Agent&                                     agent,
  Input_Feed&                                input_feed,
  const Menu_Result&                         menu_result,
  const std::string&                         window_title,
  std::function<void()>                      update_table_from_game,
  std::function<std::vector<int>()>          compute_scores,
  std::function<void()>                      update_game_from_table,
  std::function<void(const nlohmann::json&)> on_message
) {
  auto current_choice = std::optional<Choice>();

  // Nullable handle to the remote peer. Outside online mode this stays null
  // and every send/recv branch below short-circuits.
  const Online* online = menu_result.is_online() ? &menu_result.online
                                                 : nullptr;

  // Leaving playground: commit the rearranged table back into the game state
  // when the game provides a way to (gods), otherwise restore the table from
  // the canonical game state — discarding the playground edits.
  auto leave_playground = [&] {
    if (update_game_from_table) {
      update_game_from_table();
    } else if (update_table_from_game) {
      update_table_from_game();
    }
  };

  // Each frame: ask game_frame for the next move. When it resolves a choice
  // (returns nullopt), let the game-specific code refresh the table from the
  // updated game state.
  //
  // Playground mode pauses the game loop and lets the user rearrange the
  // table freely; the toggle button in the top-right corner flips it. While
  // ON the agent never sees drag/drop events, so games can't progress —
  // useful for inspecting and tinkering with state. Toggling OFF resyncs
  // the table from the canonical game state, discarding any layout edits.
  //
  // Returning true tells run_tabletop to exit the loop — we use that to
  // stop as soon as the game ends so the game-over screen can take over.
  auto update = [&](Table_State& table, const Input& input) {
    // The UI agent reads the current frame's input through ui_state.
    ui_state.input = &input;

    // Drain any messages the remote sent us this frame. Two kinds matter
    // here: a "playground" flip from the other peer (mirror it locally) and a
    // "table_state" snapshot (apply it so both screens stay in sync). Anything
    // else is handed to the game-specific on_message hook.
    if (online) {
      while (auto incoming = try_recv_message(*online)) {
        std::string type = incoming->value("type", "");
        if (type == "playground") {
          bool remote_on = incoming->value("on", false);
          if (remote_on != ui_state.playground) {
            ui_state.playground = remote_on;
            if (ui_state.playground) {
              table.is_drop_allowed = [](int, int, int) { return true; };
              ui_state.highlighted_things.clear();
            } else {
              leave_playground();
            }
          }
        } else if (type == "table_state") {
          apply_table_state_message(table, (*incoming)["table_state"]);
        } else if (on_message) {
          on_message(*incoming);
        }
      }
    }

    // Playground toggle button (top-right).
    Rectangle screen_rect = {
      0.0f, 0.0f, (float)tt::WINDOW_WIDTH, (float)tt::WINDOW_HEIGHT
    };
    Rectangle button_rect =
      place_inside(screen_rect, 160, 32, "right", "top", 20);
    std::string label = ui_state.playground ? "Playground: ON"
                                            : "Playground: OFF";
    if (immediate_button(button_rect, label, input, Color{20, 20, 20, 100})) {
      ui_state.playground = !ui_state.playground;
      if (ui_state.playground) {
        // Anything goes while playing around. Wipe the "legal move" borders
        // the agent left over the player's hand — they're misleading while
        // the game loop is paused.
        table.is_drop_allowed = [](int, int, int) { return true; };
        ui_state.highlighted_things.clear();
      } else {
        // Commit (or discard) the playground edits. The next game_frame call
        // re-installs is_drop_allowed via the agent.
        leave_playground();
      }
      // Tell the remote peer about the toggle. On entry we also blast a
      // full table snapshot so the other side starts from the same layout.
      if (online) {
        nlohmann::json msg;
        msg["type"] = "playground";
        msg["on"]   = ui_state.playground;
        send_message(*online, msg);
        if (ui_state.playground) send_table_state(*online, table);
      }
    }

    if (ui_state.playground) {
      // Replicate drop / rotate / shuffle to the remote so playground edits
      // appear on both screens. Polling the drop also drains the event so it
      // doesn't get replayed as a real move when we toggle off.
      auto dropped     = table.poll_dropped_thing();
      bool table_dirty = dropped.has_value() || key_pressed(input, KEY_R) ||
                         key_pressed(input, KEY_S);
      if (online && table_dirty) send_table_state(*online, table);
      return false;
    }

    bool state_changed = game_frame(state, agent);
    if (state_changed && update_table_from_game) {
      update_table_from_game();
    }
    return state.is_game_over();
  };

  run_tabletop(
    table, update, input_feed, tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, window_title
  );

  if (state.is_game_over() && compute_scores) {
    auto scores      = compute_scores();
    auto result_text = std::string();
    if (scores[0] > scores[1])
      result_text = "Player 1 wins!";
    else if (scores[1] > scores[0])
      result_text = "Player 2 wins!";
    else
      result_text = "It's a tie.";
    draw_game_over_screen(table, result_text, scores);
  }

  CloseWindow();
}
