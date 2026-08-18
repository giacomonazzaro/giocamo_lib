#pragma once

#include <game/agent.h>
#include <struct/json.h>
#include <tabletop/input_recorder.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>

#include "menu.h"
#include "online.h"

// SPACE-to-zoom: zooms the thing under the cursor while SPACE is held,
// clears the zoom otherwise. tabletop's process_input also handles this
// internally; calling both is harmless (idempotent).
void update_zoomed_thing(Table_State& table_state, const Input& input);

// Resolve the play mode in priority order:
//   1. `--local-host` / `--local-join` on argv → loopback handshake; no menu.
//   2. `skip_menu == true` → default Menu_Result (mode = VS_AI, no online).
//   3. Otherwise opens the menu and returns the user's choice.
// The returned Menu_Result owns its `online` field (valid only when
// mode == ONLINE). `cli_seed` is folded into the result's seed for solo
// play; online uses the matchmaker's seed instead.
Menu_Result run_menu(
  const std::string&               title,
  int                              window_width,
  int                              window_height,
  Input_Feed&                      inputs,
  std::optional<Online_Connection> local_connection,
  bool                             skip_menu,
  int                              cli_seed
);

// Wrap a local Agent into the right duel for the chosen mode:
//   - ONLINE: pairs `local_agent` with an Agent_Remote (via make_online_duel).
//   - Otherwise: returns Agent_Duel(local_agent, opponent, /*swap=*/seat1).
// For hot-seat callers pass `opponent == local_agent`; for vs-AI pass the
// already-built AI agent (caller decides any Agent_Async wrapping).
Agent* make_duel(
  Agent* local_agent, Agent* opponent, const Menu_Result& menu_result
);

// Parsed command-line options shared by every game app.
//   --hot-seat        → vs_ai=false, skip_menu=true (one screen, two players).
//   --skip-menu       → skip the menu, default to vs-AI.
//   --seed=N          → deterministic deal for solo play. When omitted, the
//                       parser generates a random seed so the field always has
//                       a value.
//   --record=PATH     → play live and write the input stream to PATH.
//   --playback=PATH   → replay the input stream in PATH instead of the mouse.
//   --load[=PATH]     → start from the game saved on disk, not from a deal.
//                       Without a path the game reads its usual one.
struct Play_Options {
  bool        vs_ai      = true;
  bool        skip_menu  = false;
  int         seed       = 0;
  Input_Mode  input_mode = Input_Mode::Live;
  std::string input_file_path;  // Where to write it, or where to read it.
  bool        load_from_disk = false;
  std::string load_path      = "data/debug_game_state.json";

  // --local-host / --local-join.
  std::optional<Online_Connection> local_connection;
};
Play_Options parse_play_args(int argc, char** argv);

// Wrap the AI in Agent_Async when playing vs the computer, then build the duel
// for the current play mode. For hot-seat, pass `local_agent` itself as the
// opponent (it plays both seats).
Agent* make_agent_pair(
  Agent*             local_agent,
  Agent*             ai_opponent,
  const Menu_Result& menu_result,
  bool               vs_ai
);

struct Agent_UI : Agent {
  Table_State table;
  // The frame being drawn, so choose_action can read the mouse. The loop sets
  // it before asking the agent for a move.
  const Input* input = nullptr;
};

struct Giocamo {
  Game&        game;
  Agent_UI&    agent_ui;
  Table_State& table;

  int  bottom_player;
  bool hot_seat;

  Giocamo(Game& game, Agent_UI& agent_ui)
      : game(game), agent_ui(agent_ui), table(agent_ui.table) {}

  virtual ~Giocamo()                      = default;
  virtual void   init_table()             = 0;
  virtual void   update_table_from_game() = 0;
  virtual Agent* agent_opponent()         = 0;
  virtual void   update_game_from_table() = 0;
  // TODO(giacomo): Move to Game
  virtual std::vector<int> player_scores() const = 0;

  virtual Agent* agent_player() { return &agent_ui; }
  virtual void   on_message(const nlohmann::json& msg) {}

  // Therse are implemented by Giocamo_With_History, no need to override.
  virtual void save_state() {}
  virtual bool undo() { return false; }
  virtual bool redo() { return false; }
  // The whole game state as JSON, and back. Online play sends this after every
  // move, and the receiving side reads it and lays the table out again.
  virtual std::string game_state_to_json() const { return ""; }
  virtual void        game_state_from_json(const std::string& json) {}
  virtual bool draw_game_editor() { return false; }
  virtual bool load_game(const std::string& path) { return false; }
};

// Standard game loop. Runs the menu, initializes the game with the seat's seed,
// lays the table out, then runs the table-top loop until the window closes.
void play_game(
  Giocamo& giocamo, Play_Options& options, const std::string& window_title
);

// Every position the game has been in, and where in that list it is now. A
// game is stateful and copyable, so a position is just a copy of it.
template <typename Game_T>
struct History {
  std::vector<Game_T> states        = {};
  int                 current_state = -1;

  void save(const Game_T& game) {
    // Playing on from an undone position drops what came after it: that
    // future is not the game's any more.
    this->states.resize(this->current_state + 1);
    this->states.push_back(game);
    this->current_state = (int)this->states.size() - 1;
  }

  bool undo(Game_T& game) {
    if (this->current_state <= 0) {
      return false;
    }
    this->current_state -= 1;
    game = this->states[current_state];
    return true;
  }

  bool redo(Game_T& game) {
    if (this->current_state + 1 >= (int)this->states.size()) {
      return false;
    }
    this->current_state += 1;
    game = this->states[current_state];
    return true;
  };
};

// A game derives from this instead of Giocamo directly to get undo. Copying a
// position needs the concrete game type, which is why this layer is a template
// — run_game still only ever sees Giocamo and calls the three hooks above.
template <typename Game_T>
struct Giocamo_With_History : Giocamo {
  using Giocamo::Giocamo;

  History<Game_T> history;

  Game_T& typed_game() { return static_cast<Game_T&>(game); }
  const Game_T& typed_game() const { return static_cast<const Game_T&>(game); }

  void save_state() override { history.save(typed_game()); }

  std::string game_state_to_json() const override {
    return to_json(typed_game(), 0, /*pretty=*/false);
  }

  // The pending choice holds functions, which JSON cannot carry, so the game
  // works it out again from the state it just read, the same way load_game
  // does.
  void game_state_from_json(const std::string& json) override {
    auto received = Game_T();
    if (!from_json(json, received)) {
      std::cerr << "could not read the game state sent by the other player\n";
      return;
    }
    typed_game() = received;
    game.begin_game();
    update_table_from_game();
  }

  bool undo() override {
    if (!history.undo(typed_game())) return false;
    update_table_from_game();
    return true;
  }

  bool redo() override {
    if (!history.redo(typed_game())) return false;
    update_table_from_game();
    return true;
  }

  bool draw_game_editor() override {
    auto edited = draw_editor_ui(typed_game());
    if (edited) update_table_from_game();
    return edited;
  }

  // --load: carry on from the snapshot. The pending choice is worked out again
  // from the phase that was saved; effects that still owed a decision are not
  // in the snapshot, so those are lost.
  bool load_game(const std::string& path) override {
    try {
      typed_game() = load_from_json<Game_T>(path);
    } catch (const std::exception& error) {
      std::cerr << error.what() << "\n";
      return false;
    }
    game.begin_game();
    return true;
  }
};