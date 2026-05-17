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

// Include everything *before* raylib — raylib defines RED/GREEN/BLUE/YELLOW as
// macros that conflict with our Card_Color enum values.
#include <game/agent.h>
#include <game/game.h>
#include <gods/ai.h>
#include <gods/gameplay.h>
#include <gods/models.h>
#include <gods/setup.h>
#ifndef __EMSCRIPTEN__
#include <online/protocol.h>
#include <online/setup.h>
#else
#include "online_stub.h"
#endif
#include <tabletop/config.h>
#include <tabletop/game_state.h>
#include <tabletop/input.h>
#include <tabletop/input_recorder.h>
#include <tabletop/models.h>
#include <tabletop/rendering.h>
#include <tabletop/ui.h>

#include <nlohmann/json.hpp>

#include "../struct/json.h"
#include "agent_remote.h"
#include "agent_ui.h"
#include "menu.h"
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
  game.all_cards.reserve(card_designs.size());
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

  game.shared_deck.reserve(game.all_cards.size());
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

  return game;
}

// Push gods_state's deck/hand/discard/peoples/wonders/shared_deck into the
// matching stacks (looked up by name), then refresh each stack's card
// positions. Reused after both fresh layout init and JSON load to keep the
// scene tree consistent with the current Game_State.
void populate_stacks_from_gods_state(
  Table_State& table_state, Game_State& gods_state
) {
  std::map<std::string, int> name_to_id;
  for (int i = 0; i < (int)table_state.things.size(); ++i) {
    const std::string& n = table_state.things[i].name;
    if (!n.empty()) name_to_id[n] = i;
  }

  for (int i = 0; i < 2; ++i) {
    const Player& p = gods_state.players[i];
    table_state.things[name_to_id["p" + std::to_string(i) + "_deck"]].children =
      p.deck;
    table_state.things[name_to_id["p" + std::to_string(i) + "_hand"]].children =
      p.hand;
    table_state.things[name_to_id["p" + std::to_string(i) + "_discard"]]
      .children = p.discard;
    table_state.things[name_to_id["p" + std::to_string(i) + "_wonders"]]
      .children = p.wonders;
    std::vector<int> peoples;
    for (int pid : gods_state.peoples) {
      if (gods_state.owner(pid) == i) peoples.push_back(pid);
    }
    table_state.things[name_to_id["p" + std::to_string(i) + "_peoples"]]
      .children = peoples;
  }
  table_state.things[name_to_id["shared_deck"]].children =
    gods_state.shared_deck;

  // Lay out cards inside each stack.
  for (int stack_id : table_state.things[table_state.root].children) {
    update_card_positions(stack_id, table_state, /*sort=*/false);
  }
}

// Build the initial Table_Layout. Things are laid out:
//   [0, num_cards)            cards aligned 1:1 with gods_state.all_cards
//   [num_cards, num_cards+11) the 11 stacks from make_gods_stacks
//   num_cards + 11            the root, whose children are all stacks
void init_table_layout(
  Table_State& table_state, Game_State& gods_state, int bottom_player = 0
) {
  // Cards aligned with all_cards so card.id is the shared key.
  for (const auto& gc : gods_state.all_cards) {
    Thing kc;
    kc.id         = gc.id;
    kc.image_path = get_image_path(card_designs[gc.id]->name);
    table_state.things.push_back(kc);
  }

  // Stack things: assign ids by append order. Track the insertion order so
  // root.children matches the original stack ordering.
  std::vector<Thing> stacks = make_gods_stacks(bottom_player);
  std::vector<int>   stack_ids_in_order;
  for (Thing& s : stacks) {
    s.id = (int)table_state.things.size();
    stack_ids_in_order.push_back(s.id);
    table_state.things.push_back(std::move(s));
  }

  // Root: sits at the end of `things`, owns all stacks as direct children.
  Thing root;
  root.name = "root";
  root.rect = {0.0f, 0.0f, (float)tt::WINDOW_WIDTH, (float)tt::WINDOW_HEIGHT};
  root.id   = (int)table_state.things.size();
  root.children = stack_ids_in_order;
  table_state.things.push_back(root);
  table_state.root = root.id;

  populate_stacks_from_gods_state(table_state, gods_state);
}

// Initialize the non-layout state on top of an already-built Table_Layout:
// num_cards and the per-card draw callbacks.
void init_table_state(
  Table_State& table_state, Game_State& gods_state, UI_State& ui_state
) {
  table_state.num_cards = (int)gods_state.all_cards.size();

  for (const auto& gc : gods_state.all_cards) {
    int id = gc.id;
    table_state.draw_callbacks[id] =
      [id, &gods_state, &ui_state](Table_State*, const Input&) {
        const auto& gcard = gods_state.all_cards[id];
        std::string power = std::to_string(gods_state.effective_power(id));
        draw_card_power_badge(power, gcard.destroyed);
        int w = tt::CARD_WIDTH;
        int h = tt::CARD_HEIGHT;
        for (const auto& [k, kt_card_id] : ui_state.highlighted_cards) {
          if (kt_card_id == id) {
            DrawRectangleRoundedLinesEx(
              Rectangle{0.0f, 0.0f, (float)w, (float)h},
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
  Table_State*                 table_state,
  Game_State&                  gods_state,
  const std::optional<Choice>& current_choice,
  UI_State&                    ui_state,
  int                          bottom_player,
  const Input&                 input
) {
  int       H      = tt::WINDOW_HEIGHT;
  int       h      = tt::CARD_HEIGHT;
  int       margin = 20;
  Rectangle window = {0.0f, 0.0f, (float)tt::WINDOW_WIDTH, (float)H};
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

  if (current_choice && !ui_state.playground) {
    const std::string& text = current_choice->text_description;
    if (!text.empty()) {
      int       font_size = 22;
      int       tw        = text_width(text, font_size);
      Rectangle r = ui_state.place(tw, font_size, "right", "center", 20);
      render_text(text, r.x, r.y - 50, font_size, Color{200, 200, 200, 255});
    }
  }

  // Playground toggle button (top-right).
  std::string label  = ui_state.playground ? "Playground: ON"
                                           : "Playground: OFF";
  Rectangle   button = ui_state.place(160, 32, "right", "top", 20);
  if (immediate_button(button, label, input, Color{20, 20, 20, 100})) {
    ui_state.playground = !ui_state.playground;
    if (!ui_state.playground) {
      sync_game_state_from_table(*table_state, gods_state);
      ui_state.power_edit_card_id = -1;
    } else {
      table_state->is_drop_card_allowed = [](int, int, int) { return true; };
      ui_state.highlighted_cards.clear();
    }
  }

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
      std::max(0.0f, std::min(panel.x, (float)(tt::WINDOW_WIDTH - panel_w)));
    panel.y = std::max(
      0.0f, std::min(panel.y, (float)(tt::WINDOW_HEIGHT - (int)panel.height))
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
      std::optional<Color> col = std::nullopt;
      if (v == current_power) col = Color{80, 160, 80, 255};
      if (immediate_button(btn, std::to_string(v), input, col)) {
        gods_state.all_cards[card_id].power = v;
        ui_state.power_edit_card_id         = -1;
      }
      btn.x += (float)(btn_w + gap);
    }
  }

  // Re-place the shared deck so it stays anchored.
  for (int child_id : table_state->things[table_state->root].children) {
    Thing& s = table_state->things[child_id];
    if (s.name == "shared_deck") {
      Rectangle target =
        ui_state.playground
          ? place_inside(
              window, tt::CARD_WIDTH, tt::CARD_HEIGHT, "right", "center", 10
            )
          : place_next(
              window, tt::CARD_WIDTH, tt::CARD_HEIGHT, "right", "center", 10
            );
      s.rect = target;
      update_card_positions(child_id, *table_state, false);
      break;
    }
  }
}

static void play_gods(
  Game_State&                 gods_state,
  Table_State&                table_state,
  UI_State&                   ui_state,
  Agent*                      agent,
  int                         player_index,
  UDP_Socket*                 sock,
  std::pair<std::string, int> friend_addr,
  Input_Recorder&             recorder,
  Agent_UI*                   agent_ui_for_input = nullptr
) {
  if (!IsWindowReady()) {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "Gods Online");
    SetTargetFPS(tt::TARGET_FPS);
  }

  std::optional<Choice> current_choice;
  bool                  prev_playground = ui_state.playground;

  // Helper: broadcast all card state to the remote player.
  auto broadcast_cards = [&]() {
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
    send_message(*sock, msg, friend_addr);
  };

  table_state.draw_callbacks[-1] = [&](Table_State* ts, const Input& input) {
    draw_hud(ts, gods_state, current_choice, ui_state, player_index, input);
  };

  // The per-frame input. Populated at the top of each frame from the recorder
  // (live capture, recording capture, or playback). Stored in an outer scope
  // so the agent (set via current_input) and HUD callback can both see it.
  Input frame_input;

  while (!WindowShouldClose()) {
    if (gods_state.game_over) break;

    frame_input = next_input(recorder);
    if (recorder.exhausted) {
      fprintf(stderr, "[input_recorder] playback exhausted, exiting\n");
      break;
    }
    if (agent_ui_for_input) agent_ui_for_input->current_input = &frame_input;

    // SPACE-to-zoom: handled by tabletop's update_input, but our outer
    // loop also peeks SPACE for the same gesture.
    if (frame_input.key_space_down) {
      auto path = find_thing_at(
        (float)frame_input.mouse_x, (float)frame_input.mouse_y, table_state
      );
      table_state.zoomed_card_id =
        (!path.empty() && is_card(table_state.things[path.back()]))
          ? std::move(path)
          : Thing_Location{};
    } else {
      table_state.zoomed_card_id.clear();
    }

    process_input(table_state, frame_input);
    int mx = frame_input.mouse_x, my = frame_input.mouse_y;

    // Playground: P opens the power editor for the hovered card.
    if (ui_state.playground && frame_input.key_p_pressed) {
      auto path = find_thing_at((float)mx, (float)my, table_state);
      if (!path.empty() && is_card(table_state.things[path.back()])) {
        int hovered = path.back();
        ui_state.power_edit_card_id =
          (ui_state.power_edit_card_id == hovered) ? -1 : hovered;
      } else {
        ui_state.power_edit_card_id = -1;
      }
    }

    // While power editor is open, 1-9 / 0 set the power directly.
    // digit_pressed is 0..8 for keys 1..9, 9 for key 0 (maps to power 10).
    if (ui_state.power_edit_card_id != -1 && frame_input.digit_pressed >= 0) {
      int i = frame_input.digit_pressed;
      gods_state.all_cards[ui_state.power_edit_card_id].power =
        (i < 9) ? (i + 1) : 10;
      ui_state.power_edit_card_id = -1;
      if (sock) broadcast_cards();
    }

    // Click-to-expand for discard stacks.
    int         discard_you      = -1;
    int         discard_opponent = -1;
    std::string discard_you_name = "p" + std::to_string(player_index) +
                                   "_discard";
    std::string discard_opponent_name = "p" + std::to_string(1 - player_index) +
                                        "_discard";
    for (int child_id : table_state.things[table_state.root].children) {
      const Thing& s = table_state.things[child_id];
      if (s.name == discard_you_name) discard_you = child_id;
      if (s.name == discard_opponent_name) discard_opponent = child_id;
    }
    if (frame_input.left_pressed) {
      for (int stack_id : {discard_opponent, discard_you}) {
        if (stack_id < 0) continue;
        Thing& s           = table_state.things[stack_id];
        bool   is_expanded = s.spread_x > 0.0f;
        bool   inside =
          point_in_stack_area((float)mx, (float)my, stack_id, table_state);
        if (inside && !is_expanded) {
          s.rect = ui_state.place(
            tt::CARD_WIDTH * 7, tt::CARD_HEIGHT, "center", "center"
          );
          s.spread_x = 150.0f;
          s.depth    = 1.0f;
          update_card_positions(stack_id, table_state, false);
        } else if (is_expanded && !inside) {
          // Restore original rect from a fresh layout. Match by name since
          // ordinal positions in make_gods_stacks aren't aligned with
          // table_state thing ids.
          auto fresh = make_gods_stacks(player_index);
          for (const Thing& f : fresh) {
            if (f.name == s.name) {
              s.rect = f.rect;
              break;
            }
          }
          s.spread_x = 0.0f;
          s.depth    = 0.0f;
          update_card_positions(stack_id, table_state, false);
        }
      }
    }

    // Helper: serialize each stack's children to a JSON array, in the order
    // stacks were appended to state.things (so indices stay stable across
    // peers).
    auto serialize_stacks = [&]() {
      nlohmann::json out = nlohmann::json::array();
      for (int i = table_state.num_cards; i < table_state.root; ++i) {
        out.push_back(table_state.things[i].children);
      }
      return out;
    };

    // Sync playground mode with remote player.
    if (sock && ui_state.playground != prev_playground) {
      nlohmann::json msg;
      msg["type"] = "playground";
      msg["on"]   = ui_state.playground;
      send_message(*sock, msg, friend_addr);
      if (ui_state.playground) {
        nlohmann::json sm;
        sm["type"]   = "stacks";
        sm["stacks"] = serialize_stacks();
        send_message(*sock, sm, friend_addr);
      }
    }
    prev_playground = ui_state.playground;

    // Send stacks if in playground/no-logic mode and something changed.
    if ((!agent || ui_state.playground) && sock) {
      auto dropped     = table_state.poll_dropped_card();
      bool should_send = dropped.has_value() || frame_input.key_r_pressed ||
                         frame_input.key_s_pressed;
      if (should_send) {
        nlohmann::json sm;
        sm["type"]   = "stacks";
        sm["stacks"] = serialize_stacks();
        send_message(*sock, sm, friend_addr);
      }
    }

    // Always receive messages from the remote player.
    if (sock) {
      auto msg_opt = try_recv_message(*sock);
      if (msg_opt) {
        const auto& msg = *msg_opt;
        std::string t   = msg.value("type", "");
        if (t == "stacks") {
          const auto& arr        = msg["stacks"];
          int         num_stacks = table_state.root - table_state.num_cards;
          for (size_t i = 0; i < arr.size() && (int)i < num_stacks; ++i) {
            int stack_id = table_state.num_cards + (int)i;
            table_state.things[stack_id].children =
              arr[i].get<std::vector<int>>();
            update_card_positions(stack_id, table_state, false);
          }
        } else if (t == "playground") {
          ui_state.playground = msg.value("on", false);
          if (ui_state.playground) {
            table_state.is_drop_card_allowed = [](int, int, int) {
              return true;
            };
            ui_state.highlighted_cards.clear();
          } else {
            sync_game_state_from_table(table_state, gods_state);
            ui_state.power_edit_card_id = -1;
          }
        } else if (t == "all_cards") {
          const auto& arr = msg["all_cards"];
          for (size_t i = 0; i < arr.size() && i < gods_state.all_cards.size();
               ++i) {
            Card& c     = gods_state.all_cards[i];
            c.power     = arr[i].value("power", c.power);
            c.counters  = arr[i].value("counters", c.counters);
            c.destroyed = arr[i].value("destroyed", c.destroyed);
            c.owner     = arr[i].value("owner", c.owner);
          }
        }
      }
    }

    BeginDrawing();

    float turn = (gods_state.current_player != player_index) ? 1.0f : 0.0f;
    draw_background(frame_input, turn);
    draw_table(table_state, frame_input);

    if (agent && !ui_state.playground) {
      bool had_choice = current_choice.has_value();
      current_choice  = game_frame(gods_state, *agent, current_choice);
      // Broadcast card mutations to the remote player after a choice resolves.
      if (sock && had_choice && !current_choice.has_value()) broadcast_cards();
      update_stacks(table_state, gods_state);
    }
    EndDrawing();
  }

  if (gods_state.game_over) {
    update_stacks(table_state, gods_state);
    std::vector<int> scores = {
      compute_player_score(gods_state, 0), compute_player_score(gods_state, 1)
    };
    std::vector<std::string> names = {
      gods_state.players[0].name, gods_state.players[1].name
    };
    std::string result_text = scores[player_index] > scores[1 - player_index]
                                ? "You win!"
                              : scores[player_index] < scores[1 - player_index]
                                ? "You lose!"
                                : "It's a tie!";
    draw_game_over_screen(table_state, result_text, names, scores);
  }

  CloseWindow();
}

int main(int argc, char** argv) {
  // CLI: default is to show the menu. `agent` flag plays vs AI as player 0
  // without showing the menu.
  bool               skip_menu_vs_ai = false;
  std::optional<int> seed;
  Input_Mode         input_mode = Input_Mode::Live;
  std::string        input_file_path;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "agent")
      skip_menu_vs_ai = true;
    else if (a.rfind("--seed=", 0) == 0)
      seed = std::atoi(a.c_str() + 7);
    else if (a.rfind("--record=", 0) == 0) {
      input_mode      = Input_Mode::Record;
      input_file_path = a.substr(9);
    } else if (a.rfind("--playback=", 0) == 0) {
      input_mode      = Input_Mode::Playback;
      input_file_path = a.substr(11);
    }
  }

  Menu_Result menu_result;
  if (skip_menu_vs_ai) {
    menu_result.mode = Menu_Result::VS_AI;
  } else {
    menu_result = run_menu();
  }

  int                         player_index = menu_result.player_index;
  UDP_Socket*                 sock         = nullptr;
  std::pair<std::string, int> friend_addr;
  std::shared_ptr<UDP_Socket> sock_holder;

  if (menu_result.mode == Menu_Result::ONLINE) {
    sock_holder = menu_result.sock;
    sock        = sock_holder.get();
    friend_addr = menu_result.friend_addr;
    seed        = menu_result.seed;
  }

#if 0
  Game_State gods_state = quick_setup(seed);
  save_to_json(gods_state, "data/debug_gods_state.json");
#else
  Game_State gods_state =
    load_from_json<Game_State>("data/debug_gods_state.json");
#endif

  UI_State ui_state;
#if 0
  auto table_state = Table_State();
  init_table_layout(table_state, gods_state, player_index);
  init_table_state(table_state, gods_state, ui_state);
  save_to_json(*(Table_Layout*)&table_state, "data/debug_table_state.json");
#else
  auto table_layout =
    load_from_json<Table_Layout>("data/debug_table_state.json");
  auto table_state = Table_State(table_layout);
  populate_stacks_from_gods_state(table_state, gods_state);
  init_table_state(table_state, gods_state, ui_state);
#endif

  Agent_UI                      agent_ui(&table_state, &ui_state, player_index);
  std::unique_ptr<Agent>        ai_opponent;
  std::unique_ptr<Agent_Remote> remote_opponent;
  std::unique_ptr<Agent_Local_Online> wrap_local;

  Agent* agent_local    = &agent_ui;
  Agent* agent_opponent = nullptr;

  if (sock) {
    wrap_local =
      std::make_unique<Agent_Local_Online>(&agent_ui, sock, friend_addr);
    remote_opponent = std::make_unique<Agent_Remote>(sock);
    agent_local     = wrap_local.get();
    agent_opponent  = remote_opponent.get();
  } else if (menu_result.mode == Menu_Result::VS_AI) {
    ai_opponent    = std::make_unique<Agent_Minimax_Stochastic_Gods>(6, 20);
    agent_opponent = ai_opponent.get();
  } else {
    agent_opponent = &agent_ui;  // hot-seat
  }

  Agent_Duel duel(agent_local, agent_opponent, /*swap=*/player_index != 0);

  Input_Recorder recorder;
  init_recorder(recorder, input_mode, input_file_path);

  play_gods(
    gods_state,
    table_state,
    ui_state,
    &duel,
    player_index,
    sock,
    friend_addr,
    recorder,
    &agent_ui
  );

  finalize_recorder(recorder);
  return 0;
}
