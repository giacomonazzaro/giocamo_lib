#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// Include gods/ headers before anything that transitively pulls raylib —
// raylib defines RED/GREEN/BLUE/YELLOW as macros and that conflicts with
// our Card_Color enum values, so the enum has to be parsed first.
#include <gods/ai.h>
#include <gods/gameplay.h>
#include <gods/models.h>
#include <gods/setup.h>
//
#include <game/agent.h>
#include <game/game.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <online/agents.h>
#include <online/protocol.h>
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <nlohmann/json.hpp>

#include "../struct/json.h"
#include "../tabletop/tabletop_json.h"
#include "agent_ui.h"
#include "ui.h"

// raylib last; its color-name macros (RED/GREEN/BLUE/...) would otherwise
// expand inside Card_Color and break the enum.
#include <raylib.h>

namespace fs_helpers {

// Load and apply cards.json → C++ card_designs registry.
// Path is gods/cards.json relative to the current working directory.
static void load_card_designs() {
  std::ifstream f("gods/cards.json");
  if (!f) {
    std::cerr << "Could not open gods/cards.json\n";
    std::exit(1);
  }
  nlohmann::json data;
  f >> data;

  std::vector<std::tuple<std::string, std::string, std::string, std::string>>
    entries;
  for (size_t i = 0; i < data.size(); ++i) {
    const auto& d = data[i];
    entries.emplace_back(
      std::to_string(i),
      d.value("type", ""),
      d.value("color", ""),
      d.value("effect", "")
    );
  }
  set_card_designs(entries);
}

}  // namespace fs_helpers

static int random_int(std::mt19937& rng, int lo, int hi) {
  return std::uniform_int_distribution<int>(lo, hi)(rng);
}

// All cards drawn from the shared deck; players start with 5-card hands.
static Game_State quick_setup(std::optional<int> seed) {
  std::mt19937 rng(seed ? *seed : std::random_device{}());
  fs_helpers::load_card_designs();

  Game_State game;

  // Build all_cards from card_designs.
  for (const auto& d : card_designs) {
    Card c;
    c.id        = d->id;
    c.card_type = d->card_type;
    c.color     = d->color;
    c.power     = random_int(rng, 1, 5);
    game.all_cards.push_back(c);
  }

  // Two empty players; shared deck holds every card id, shuffled.
  Player p0;
  p0.name = "Player 1";
  Player p1;
  p1.name      = "Player 2";
  game.players = {p0, p1};
  game.peoples = {};

  for (const auto& c : game.all_cards) game.shared_deck.push_back(c.id);
  std::shuffle(game.shared_deck.begin(), game.shared_deck.end(), rng);

  // Deal opening 5-card hands.
  for (Player& p : game.players) {
    for (int i = 0; i < 5; ++i) {
      if (game.shared_deck.empty()) break;
      int cid = game.shared_deck.back();
      game.shared_deck.pop_back();
      p.hand.push_back(cid);
    }
  }

  game.begin_game(game.next_choice());  // The opening decision to present.
  return game;
}

// Push gods_state's deck/hand/discard/peoples/wonders/shared_deck into the
// matching stacks (looked up by name), then refresh each stack's card
// positions. Reused after both fresh layout init and JSON load to keep the
// scene tree consistent with the current Game_State.
void populate_stacks_from_gods_state(
  Table_State& table_state, Game_State& gods_state
) {
  auto thing_id = std::map<std::string, int>();
  for (int i = 0; i < (int)table_state.things.size(); ++i) {
    const std::string& n = table_state.things[i].name;
    if (!n.empty()) thing_id[n] = i;
  }

  auto set_children = [&](const std::string& name, array<const int> ids) {
    table_state.things[thing_id[name]]._children.assign(
      ids.data, ids.data + ids.size()
    );
  };
  for (int i = 0; i < 2; ++i) {
    const Player& p  = gods_state.players[i];
    auto          pp = "p" + std::to_string(i);
    set_children(pp + "_deck", p.deck);
    set_children(pp + "_hand", p.hand);
    set_children(pp + "_discard", p.discard);
    set_children(pp + "_wonders", p.wonders);
    std::vector<int> peoples;
    for (int pid : gods_state.peoples) {
      if (gods_state.owner(pid) == i) peoples.push_back(pid);
    }
    set_children(pp + "_peoples", peoples);
  }
  set_children("shared_deck", gods_state.shared_deck);

  // Lay out cards inside each stack.
  for (int stack_id : table_state.things[table_state.root].children()) {
    update_children_positions(stack_id, table_state, /*sort=*/false);
  }
}

// Build the initial Table_Layout. Things are laid out:
//   [0, N)                    cards aligned 1:1 with gods_state.all_cards
//   [N, N+11)                 the 11 stacks from make_gods_stacks
//   N + 11                    the root, whose children are all stacks
void init_table_layout(
  Table_State& table_state,
  Game_State&  gods_state,
  int          bottom_player,
  int          window_width,
  int          window_height
) {
  table_state.width  = window_width;
  table_state.height = window_height;

  // Cards aligned with all_cards so card.id is the shared key.
  for (const auto& gc : gods_state.all_cards) {
    auto  image_path = get_image_path(card_designs[gc.id]->name);
    Thing card       = make_card(gc.id, image_path);
    table_state.things.push_back(card);
  }

  // Stack things: assign ids by append order. Track the insertion order so
  // root.children() matches the original stack ordering. make_gods_stacks
  // returns stacks in root-local coords (root is centered on the screen).
  std::vector<Thing> stacks =
    make_gods_stacks(bottom_player, window_width, window_height);
  std::vector<int> stack_ids_in_order;
  for (Thing& s : stacks) {
    s.id = (int)table_state.things.size();
    stack_ids_in_order.push_back(s.id);
    table_state.things.push_back(std::move(s));
  }

  // Root: sits at the end of `things`, owns all stacks as direct children.
  // Centered on the screen so the root rect spans (0,0)-(W,H) in world.
  Thing root;
  root.name        = "root";
  root.shape       = rectangle_shape({(float)window_width, (float)window_height});
  root.transform.x = (float)window_width / 2.0f;
  root.transform.y = (float)window_height / 2.0f;
  root.id          = (int)table_state.things.size();
  root._children    = stack_ids_in_order;
  root.capacity    = 0;
  // Transparent so the shader background drawn behind the table shows through.
  root.color       = {0, 0, 0, 0};
  table_state.things.push_back(root);
  table_state.root = root.id;

  populate_stacks_from_gods_state(table_state, gods_state);
}

// Initialize the non-layout state on top of an already-built Table_Layout:
// the per-card draw callbacks.
void init_card_draw_callbacks(
  Table_State&      table_state,
  const Game_State& gods_state,
  const UI_State&   ui_state
) {
  for (const auto& gc : gods_state.all_cards) {
    int id = gc.id;
    table_state.draw_callbacks[id] =
      [id,
       &gods_state,
       &ui_state](const Table_State&, const Input&, bool face_up) {
        const auto& gcard = gods_state.all_cards[id];
        std::string power = std::to_string(gods_state.effective_power(id));
        if (face_up) {
          draw_card_power_badge(power, gcard.destroyed);
        }
        // Drawn in card-local space where the card center is at (0, 0).
        int w = tt::CARD_WIDTH;
        int h = tt::CARD_HEIGHT;
        for (const auto& [k, kt_card_id] : ui_state.highlighted_things) {
          if (kt_card_id == id) {
            DrawRectangleRoundedLinesEx(
              Rectangle{-(float)w / 2.0f, -(float)h / 2.0f, (float)w, (float)h},
              0.25f,
              8,
              4.0f,
              ::Color{255, 215, 0, 200}
            );
            break;
          }
        }
      };
  }
}

// Per-frame HUD overlay drawn by draw_table via Table_State::draw_callback.
static void draw_hud(
  Table_State* table_state,
  Game_State&  gods_state,
  Gods_UI&     ui_state,
  int          bottom_player,
  const Input& input
) {
  int       H      = table_state->height;
  int       h      = tt::CARD_HEIGHT;
  int       margin = 20;
  Rectangle window = {0.0f, 0.0f, (float)table_state->width, (float)H};
  int       bottom_wonders_y =
    (int)place_inside(window, 0, h, "left", "bottom", 2 * margin + h).y;
  int opponent_shift = (int)(h * 0.65f);
  int top_wonders_y  = H - bottom_wonders_y - h - opponent_shift;

  for (int i = 0; i < 2; ++i) {
    int  score      = compute_player_score(gods_state, i);
    bool is_current = (i == gods_state.current_player);
    int  hud_y      = bottom_wonders_y + h / 2;
    if (i != bottom_player) hud_y = top_wonders_y + h / 2;
    draw_player_hud(
      i, score, (int)gods_state.players[i].deck.size(), is_current, hud_y
    );
  }

  ui_state.draw_buttons(input);

  // Power editor overlay.
  int card_id = ui_state.power_edit_card_id;
  if (card_id != -1 && ui_state.playground) {
    int btn_w = 44, btn_h = 36, gap = 6;
    int panel_w = 10 * btn_w + 9 * gap + 16;
    // Place panel relative to the card's WORLD position (rect is local).
    Rectangle card_rect = world_rect(card_id, *table_state);
    card_rect.width     = (float)tt::CARD_WIDTH;
    card_rect.height    = (float)tt::CARD_HEIGHT;
    Rectangle panel =
      place_next(card_rect, panel_w, btn_h + 16, "center", "bottom", 8);
    panel.x =
      std::max(0.0f, std::min(panel.x, (float)(table_state->width - panel_w)));
    panel.y = std::max(
      0.0f, std::min(panel.y, (float)(table_state->height - (int)panel.height))
    );
    DrawRectangleRounded(
      Rectangle{panel.x, panel.y, panel.width, panel.height},
      0.3f,
      8,
      ::Color{20, 20, 20, 200}
    );
    Rectangle btn = place_inside(panel, btn_w, btn_h, "left", "center", 8);
    int       current_power = gods_state.all_cards[card_id].power;
    for (int v = 1; v <= 10; ++v) {
      Color col = s_button_color;
      if (v == current_power) col = Color{80, 160, 80, 255};
      if (immediate_button(btn, std::to_string(v), input, col)) {
        gods_state.all_cards[card_id].power = v;
        ui_state.power_edit_card_id         = -1;
      }
      btn.x += (float)(btn_w + gap);
    }
  }

  // Re-place the shared deck so it stays anchored. The stack is a root-child
  // so the placement is computed in root-local coords (root is centered, so
  // the window spans (-W/2, -H/2) to (W/2, H/2) in that space).
  float     W           = (float)table_state->width;
  float     Hf          = (float)H;
  Rectangle root_window = {-W / 2.0f, -Hf / 2.0f, W, Hf};
  for (int child_id : table_state->things[table_state->root].children()) {
    Thing& s = table_state->things[child_id];
    if (s.name == "shared_deck") {
      Rectangle target = ui_state.playground ? place_inside(
                                                 root_window,
                                                 tt::CARD_WIDTH,
                                                 tt::CARD_HEIGHT,
                                                 "right",
                                                 "center",
                                                 10
                                               )
                                             : place_next(
                                                 root_window,
                                                 tt::CARD_WIDTH,
                                                 tt::CARD_HEIGHT,
                                                 "right",
                                                 "center",
                                                 10
                                               );
      set_local_rect(s, target);
      update_children_positions(child_id, *table_state, false);
      break;
    }
  }
}

// P opens/closes the power editor for the hovered card (playground only).
// While the editor is open, 1-9 / 0 set the power to 1-10. Returns true if
// a card's power was actually changed this frame.
static bool handle_power_editor(
  Game_State&  gods_state,
  Table_State& table_state,
  Gods_UI&     ui_state,
  const Input& input
) {
  if (ui_state.playground && key_pressed(input, KEY_P)) {
    auto path =
      find_thing_at((float)input.mouse_x, (float)input.mouse_y, table_state);
    if (!path.empty()) {
      int hovered = path.back();
      ui_state.power_edit_card_id =
        (ui_state.power_edit_card_id == hovered) ? -1 : hovered;
    } else {
      ui_state.power_edit_card_id = -1;
    }
  }
  if (ui_state.power_edit_card_id == -1) return false;

  // KEY_ONE..KEY_NINE map to powers 1..9; KEY_ZERO maps to 10.
  const int digit_keys[10] = {
    KEY_ONE,
    KEY_TWO,
    KEY_THREE,
    KEY_FOUR,
    KEY_FIVE,
    KEY_SIX,
    KEY_SEVEN,
    KEY_EIGHT,
    KEY_NINE,
    KEY_ZERO,
  };
  for (int i = 0; i < 10; ++i) {
    if (key_pressed(input, digit_keys[i])) {
      gods_state.all_cards[ui_state.power_edit_card_id].power =
        (i < 9) ? (i + 1) : 10;
      ui_state.power_edit_card_id = -1;
      return true;
    }
  }
  return false;
}

// D writes a debug snapshot of gods_state + table_state to data/debug_*.json.
static void handle_debug_save(
  Table_State& table_state, Game_State& gods_state, const Input& input
) {
  if (!key_pressed(input, KEY_D)) return;
  // Push any unsynced layout changes (e.g. playground rearrangement)
  // back into gods_state so the two snapshots agree — otherwise the
  // load path's per-frame update_stacks would snap cards back to
  // whatever gods_state.players[*] says.
  sync_game_state_from_table(
    table_state, gods_state, (int)gods_state.all_cards.size()
  );
  save_to_json<Game_State>(gods_state, "data/debug_gods_state.json");
  save_to_json<Table_Layout>(table_state, "data/debug_table_state.json");
  printf("Saved debug snapshot to data/debug_*.json\n");
}

// Click-to-expand for the player's own and the opponent's discard stacks.
static void handle_discard_expand(
  Table_State& table_state,
  Gods_UI&     ui_state,
  int          player_index,
  const Input& input
) {
  // Click-to-expand for discard stacks.
  int         discard_you      = -1;
  int         discard_opponent = -1;
  std::string discard_you_name = "p" + std::to_string(player_index) +
                                 "_discard";
  std::string discard_opponent_name = "p" + std::to_string(1 - player_index) +
                                      "_discard";
  for (int child_id : table_state.things[table_state.root].children()) {
    const Thing& s = table_state.things[child_id];
    if (s.name == discard_you_name) discard_you = child_id;
    if (s.name == discard_opponent_name) discard_opponent = child_id;
  }
  if (!input.left_pressed) return;
  int mx = input.mouse_x, my = input.mouse_y;
  for (int stack_id : {discard_opponent, discard_you}) {
    if (stack_id < 0) continue;
    Thing& s           = table_state.things[stack_id];
    bool   is_expanded = s.spread_x > 0.0f;
    bool   inside = point_in_thing((float)mx, (float)my, stack_id, table_state);
    if (inside && !is_expanded) {
      // ui_state.place returns world coords; shift into root-local since
      // s is a root child.
      const Transform2D& root_world =
        table_state.things[table_state.root].transform;
      Rectangle target =
        ui_state.place(tt::CARD_WIDTH * 7, tt::CARD_HEIGHT, "center", "center");
      target.x -= root_world.x;
      target.y -= root_world.y;
      set_local_rect(s, target);
      s.spread_x = 150.0f;
      s.depth    = 1.0f;
      update_children_positions(stack_id, table_state, false);
    } else if (is_expanded && !inside) {
      // Restore original rect from a fresh layout. Match by name since
      // ordinal positions in make_gods_stacks aren't aligned with
      // table_state thing ids.
      auto fresh =
        make_gods_stacks(player_index, table_state.width, table_state.height);
      for (const Thing& f : fresh) {
        if (f.name == s.name) {
          s.transform = f.transform;
          s.shape     = f.shape;
          break;
        }
      }
      s.spread_x = 0.0f;
      s.depth    = 0.0f;
      update_children_positions(stack_id, table_state, false);
    }
  }
}

// Broadcast all card state to the remote player.
static void broadcast_cards(
  const Online& online, const Game_State& gods_state
) {
  nlohmann::json cards = nlohmann::json::array();
  for (const Card& c : gods_state.all_cards) {
    nlohmann::json j;
    j["power"]     = c.power;
    j["counters"]  = c.counters;
    j["destroyed"] = c.destroyed;
    j["owner"]     = c.owner;
    cards.push_back(j);
  }
  nlohmann::json msg;
  msg["type"]      = "all_cards";
  msg["all_cards"] = cards;
  send_message(online, msg);
}

// All CLI options understood by the binary.
struct Cli_Args {
  bool        skip_menu_vs_ai = false;
  Input_Mode  input_mode      = Input_Mode::Live;
  std::string input_file_path;  // For Record/Playback.
  int         window_width  = tt::WINDOW_WIDTH;
  int         window_height = tt::WINDOW_HEIGHT;
  // When true, boot from data/debug_*.json (the saved layout + game state).
  // Default is a fresh deal via quick_setup; --load opts into the snapshot.
  bool load_from_disk = false;
  // --hot-seat: two humans share the screen. Skips the menu and routes the
  // opponent seat to the same Agent_UI as the local one.
  bool hot_seat = false;
};
VISITABLE_STRUCT(
  Cli_Args,
  skip_menu_vs_ai,
  input_mode,
  input_file_path,
  window_width,
  window_height,
  load_from_disk,
  hot_seat
);

static Cli_Args parse_cli_args(int argc, char** argv) {
  Cli_Args args;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "agent") {
      args.skip_menu_vs_ai = true;
    } else if (a == "--load") {
      args.load_from_disk = true;
    } else if (a == "--hot-seat") {
      args.hot_seat = true;
    } else if (a.rfind("--record=", 0) == 0) {
      args.input_mode      = Input_Mode::Record;
      args.input_file_path = a.substr(9);
    } else if (a.rfind("--playback=", 0) == 0) {
      args.input_mode      = Input_Mode::Playback;
      args.input_file_path = a.substr(11);
    } else if (a.rfind("--width=", 0) == 0) {
      args.window_width = std::stoi(a.substr(8));
    } else if (a.rfind("--height=", 0) == 0) {
      args.window_height = std::stoi(a.substr(9));
    }
  }
  print(args);
  return args;
}

static void init_table_and_gods_states(
  Game_State&        gods_state,
  Table_State&       table_state,
  int                bottom_player,
  int                window_width,
  int                window_height,
  bool               load_from_disk,
  std::optional<int> seed
) {
  // Card hooks (Card::on_played etc.) dispatch through the global card_designs
  // registry, so it must be populated before any gameplay runs — regardless of
  // whether the game state comes from quick_setup or from a JSON snapshot.
  fs_helpers::load_card_designs();
  if (!load_from_disk) {
    // Online peers pass the same seed (negotiated during the handshake) so
    // both sides shuffle the deck identically.
    gods_state = quick_setup(seed);
    init_table_layout(
      table_state, gods_state, bottom_player, window_width, window_height
    );
  } else {
    gods_state  = load_from_json<Game_State>("data/debug_gods_state.json");
    auto layout = load_from_json<Table_Layout>("data/debug_table_state.json");
    table_state = Table_State(window_width, window_height, layout);
  }
}

int main(int argc, char** argv) {
  Cli_Args args = parse_cli_args(argc, argv);

  auto inputs = Input_Feed(args.input_mode, args.input_file_path);

  // run_menu folds in --local-host / --local-join, the skip-menu fallback, and
  // the menu itself. "agent" and --hot-seat both skip the menu (vs-AI duel).
  bool skip_menu   = args.skip_menu_vs_ai || args.hot_seat;
  auto menu_result = run_menu(
    "Gods",
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    inputs,
    argc,
    argv,
    skip_menu,
    /*cli_seed=*/(int)std::random_device{}()
  );

  // nullptr means local-only; otherwise borrow the bundle from menu_result.
  const Online* online = menu_result.is_online() ? &menu_result.online
                                                 : nullptr;

  Game_State  gods_state;
  Table_State table_state;
  init_table_and_gods_states(
    gods_state,
    table_state,
    menu_result.player_index,
    tt::WINDOW_WIDTH,
    tt::WINDOW_HEIGHT,
    args.load_from_disk,
    menu_result.seed
  );

  Gods_UI ui_state(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);
  init_card_draw_callbacks(table_state, gods_state, ui_state);

  const int player_index  = menu_result.player_index;
  const int stacks_offset = (int)gods_state.all_cards.size();

  // UI agent for the local seat; AI for the opponent unless hot-seat (where the
  // UI agent plays both seats). make_duel wires up the online/local pairing.
  Agent* agent_ui = new Agent_UI(
    &table_state, &ui_state, player_index, stacks_offset
  );
  bool   vs_ai    = !args.hot_seat && menu_result.mode == Menu_Result::VS_AI;
  Agent* opponent =
    vs_ai ? (Agent*)new Agent_Minimax_Stochastic_Gods(6, 20) : agent_ui;
  Agent* agent = make_duel(agent_ui, opponent, menu_result);

  // Per-frame overlay: gods-specific inputs (power editor, debug save, discard
  // expand) plus the HUD drawing. The -1 callback runs every frame with the
  // current input, so all the per-frame gods logic lives here.
  table_state.draw_callbacks[-1] =
    [&](const Table_State&, const Input& input, bool) {
      if (handle_power_editor(gods_state, table_state, ui_state, input)) {
        if (online) broadcast_cards(*online, gods_state);
      }
      handle_debug_save(table_state, gods_state, input);
      handle_discard_expand(table_state, ui_state, player_index, input);
      draw_hud(&table_state, gods_state, ui_state, player_index, input);
    };

  // Refresh the table from gods_state after every resolved choice, and push
  // the latest card state (power/owner/...) to the remote peer.
  auto update_table_from_game = [&] {
    update_stacks(table_state, gods_state, stacks_offset);
    if (online) broadcast_cards(*online, gods_state);
  };

  // Leaving playground: read the rearranged table back into gods_state so play
  // resumes from the user's layout.
  auto update_game_from_table = [&] {
    sync_game_state_from_table(table_state, gods_state, stacks_offset);
    ui_state.power_edit_card_id = -1;
  };

  // Gods-specific online message: full card state from the remote peer.
  auto on_message = [&](const nlohmann::json& msg) {
    if (msg.value("type", "") != "all_cards") return;
    const auto& arr = msg["all_cards"];
    for (size_t i = 0; i < arr.size() && i < gods_state.all_cards.size(); ++i) {
      Card& c     = gods_state.all_cards[i];
      c.power     = arr[i].value("power", c.power);
      c.counters  = arr[i].value("counters", c.counters);
      c.destroyed = arr[i].value("destroyed", c.destroyed);
      c.owner     = arr[i].value("owner", c.owner);
    }
  };

  play_game(
    gods_state,
    table_state,
    ui_state,
    *agent,
    inputs,
    menu_result,
    "Gods",
    update_table_from_game,
    [&] {
      return std::vector<int>{
        compute_player_score(gods_state, 0),
        compute_player_score(gods_state, 1),
      };
    },
    update_game_from_table,
    on_message
  );

  finalize_input_recorder(inputs);
  return 0;
}
