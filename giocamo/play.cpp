#include "play.h"

#include <raylib.h>
#include <tabletop/config.h>
#include <tabletop/editor.h>
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

// The whole game state, sent after every move the local player makes and after
// every undo, so the other player reads it and lays the table out again.
static void send_game_state(const Online& online, const Giocamo& giocamo) {
  auto game_state = giocamo.game_state_to_json();
  // A game that cannot be written as JSON sends nothing. The other player then
  // moves forward on the action index alone, which Agent_Remote already sends.
  if (game_state.empty()) return;
  nlohmann::json message;
  message["type"]       = "game_state";
  message["game_state"] = game_state;
  send_message(online, message);
}

Menu_Result run_menu(
  const std::string&               title,
  int                              window_width,
  int                              window_height,
  Input_Feed&                      inputs,
  std::optional<Online_Connection> local_connection,
  bool                             skip_menu,
  int                              cli_seed
) {
  // --local-host / --local-join skip the menu: the room code is fixed.
  if (local_connection) {
    Menu_Result result;
    result.mode         = Menu_Result::ONLINE;
    result.online       = local_connection->online;
    result.player_index = local_connection->player_index;
    result.seed         = local_connection->seed;
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

static void draw_game_over_screen(const Giocamo& giocamo) {
  auto scores      = giocamo.player_scores();
  auto result_text = "";
  if (scores[0] > scores[1])
    result_text = "Player 1 wins!";
  else if (scores[1] > scores[0])
    result_text = "Player 2 wins!";
  else
    result_text = "It's a tie.";

  const int   W          = tt::WINDOW_WIDTH;
  const int   H          = tt::WINDOW_HEIGHT;
  const char* title      = "GAME OVER";
  std::string score_line = std::to_string(scores[0]) + " - " +
                           std::to_string(scores[1]);

  {
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
    } else if (arg.rfind("--record=", 0) == 0) {
      options.input_mode      = Input_Mode::Record;
      options.input_file_path = arg.substr(9);
    } else if (arg.rfind("--playback=", 0) == 0) {
      options.input_mode      = Input_Mode::Playback;
      options.input_file_path = arg.substr(11);
    } else if (arg == "--load") {
      options.load_from_disk = true;
    } else if (arg.rfind("--load=", 0) == 0) {
      options.load_from_disk = true;
      options.load_path      = arg.substr(7);
    }
  }
  // Make seed always carry a real value so callers never have to think about
  // "is this set?". A fresh random one is picked when --seed isn't passed.
  if (!seed_given) {
    options.seed = (int)std::random_device{}();
  }

  options.local_connection = setup_local_from_argv(argc, argv);
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
  Agent* opponent = vs_ai ? ai_opponent : local_agent;
  return make_duel(local_agent, opponent, menu_result);
}

// The loop both call shapes share.
static void run_game(
  Giocamo&           giocamo,
  Input_Feed&        input_feed,
  Agent&             agent,
  const Online*      online,
  const std::string& window_title
) {
  auto& state      = giocamo.game;
  auto& table      = giocamo.table;
  bool  playground = false;

  giocamo.update_table_from_game();
  giocamo.save_state();  // The opening position, so undo has a floor.

  // Leaving playground: commit the rearranged table back into the game state
  // when the game provides a way to, otherwise restore the table from
  // the canonical game state — discarding the playground edits.
  auto leave_playground = [&] {
    giocamo.update_game_from_table();
    // giocamo.update_table_from_game();
  };

  // Returning true tells run_tabletop to exit the loop — we use that to
  // stop as soon as the game ends so the game-over screen can take over.
  auto update = [&](Table_State& table, const Input& input) -> bool {
    // The UI agent reads the current frame through the input it is handed.
    giocamo.agent_ui.input = &input;

    // Drain any messages the remote sent us this frame. Two kinds matter
    // here: a "game_state" from the other player (read it and lay the table out
    // again) and a "playground" flip (mirror it locally). Anything else is
    // handed to the game-specific on_message hook.
    if (online) {
      while (auto incoming = try_recv_message(*online)) {
        std::string type = incoming->value("type", "");
        if (type == "game_state") {
          giocamo.game_state_from_json(incoming->value("game_state", ""));
          // The state already contains the move, and the action index that
          // came before it is still queued. Agent_Remote must not resolve it a
          // second time, so it is thrown away here.
          while (try_recv_message(*online, "action")) {
          }
        } else if (type == "playground") {
          bool remote_on = incoming->value("on", false);
          if (remote_on != playground) {
            playground = remote_on;
            if (playground) {
              table.is_drop_allowed = [](int, int, int) { return true; };
            } else {
              leave_playground();
            }
          }
        } else {
          giocamo.on_message(*incoming);
        }
      }
    }

    // Playground toggle button (top-right).
    Rectangle screen_rect = {
      0.0f, 0.0f, (float)tt::WINDOW_WIDTH, (float)tt::WINDOW_HEIGHT
    };
    Rectangle button_rect =
      place_inside(screen_rect, 160, 32, "right", "top", 20);
    std::string label = playground ? "Playground: ON" : "Playground: OFF";
    if (immediate_button(button_rect, label, input, Color{20, 20, 20, 100})) {
      playground = !playground;
      if (playground) {
        table.is_drop_allowed = [](int, int, int) { return true; };
      } else {
        leave_playground();
      }
      // Tell the remote peer about the toggle. Nothing has changed yet, so
      // there is nothing else to send: both tables are laid out from the
      // same game state.
      if (online) {
        nlohmann::json msg;
        msg["type"] = "playground";
        msg["on"]   = playground;
        send_message(*online, msg);
      }
    }

    if (playground) {
      auto table_edited = draw_editor_ui(giocamo.table, input);

      // Replicate drop / rotate / shuffle to the remote so playground edits
      // appear on both screens. Polling the drop also drains the event so it
      // doesn't get replayed as a real move when we toggle off.
      auto dropped = giocamo.table.poll_dropped_thing();
      table_edited |= dropped.has_value();
      table_edited |= key_pressed(input, KEY_R);
      table_edited |= key_pressed(input, KEY_S);

      auto game_edited = giocamo.draw_game_editor();
      if (game_edited) giocamo.update_table_from_game();

      // Moving things on the table is the edit, but the game state is what
      // travels: it is worked out from the table and sent, and the other
      // player lays its own table out from it.
      if (online && table_edited) {
        giocamo.update_game_from_table();
        send_game_state(*online, giocamo);
      }
      // Editing the game state by hand is a change like any other move.
      if (online && game_edited) {
        send_game_state(*online, giocamo);
      }
      return false;
    }

    // Step back and forth through the positions played. The agents keep a
    // search keyed on the pending choice, so a jump has to clear it: the
    // restored choice can look like the one an agent is thinking about while
    // the position behind it is a different one.
    if (key_pressed(input, KEY_Z) && giocamo.undo()) {
      agent.reset();
      if (online) send_game_state(*online, giocamo);
    }
    if (key_pressed(input, KEY_X) && giocamo.redo()) {
      agent.reset();
      if (online) send_game_state(*online, giocamo);
    }

    // The game is over: its screen is drawn here, frame after frame, like any
    // other one. The loop ends when the window does.
    if (state.is_game_over()) {
      draw_game_over_screen(giocamo);
      return false;
    }

    bool is_action_from_player = state._choice.player_index ==
                                 giocamo.bottom_player;
    bool was_action_taken = game_frame(state, agent);
    if (was_action_taken) {
      giocamo.update_table_from_game();
      if (is_action_from_player) {
        giocamo.save_state();
        // The other player resolved the same action from the index it was
        // sent, but the state says so directly, and it also covers whatever
        // the action set that the index alone does not.
        if (online) send_game_state(*online, giocamo);
      }
    }

    // Without a score screen to show, the game ending ends the loop. With one,
    // the loop goes on and the branch above draws it from the next frame.
    return state.is_game_over();  // && !giocamo.player_scores;
  };

  run_tabletop(
    table, update, input_feed, tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, window_title
  );

  // The menu, the game and the game-over screen all drew into one window;
  // this is the end of all of them.
  close_table_window();
}

void play_game(
  Giocamo& giocamo, Play_Options& options, const std::string& window_title
) {
  auto input_feed  = Input_Feed(options.input_mode, options.input_file_path);
  auto menu_result = run_menu(
    window_title,
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    input_feed,
    options.local_connection,
    options.skip_menu,
    options.seed
  );

  // The local player sits at the bottom; in online play that may be seat 1.
  // Both hands are shown only in hot-seat, where one screen is shared.
  giocamo.bottom_player = menu_result.player_index;
  giocamo.hot_seat      = !options.vs_ai && !menu_result.is_online();

  // A saved game stands in for the deal when one was asked for and found.
  // Either way the game holds a position before the table is laid out over it.
  if (!(options.load_from_disk && giocamo.load_game(options.load_path))) {
    giocamo.game.init(menu_result.seed);
  }
  giocamo.init_table();

  Agent* agent = make_agent_pair(
    giocamo.agent_player(), giocamo.agent_opponent(), menu_result, options.vs_ai
  );

  // Nullable handle to the remote peer. Outside online mode this stays null
  // and every send/recv branch below short-circuits.
  const Online* online = menu_result.is_online() ? &menu_result.online
                                                 : nullptr;

  run_game(giocamo, input_feed, *agent, online, window_title);
}
