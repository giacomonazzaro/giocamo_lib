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
#include <game_cpp/agent.h>
#include <game_cpp/game.h>
#include <gods_cpp/code/ai.h>
#include <gods_cpp/code/gameplay.h>
#include <gods_cpp/code/models.h>
#include <gods_cpp/code/setup.h>
#include <kitchen_table_cpp/code/config.h>
#include <kitchen_table_cpp/code/game_state.h>
#include <kitchen_table_cpp/code/input.h>
#include <kitchen_table_cpp/code/models.h>
#include <kitchen_table_cpp/code/rendering.h>
#include <kitchen_table_cpp/code/ui.h>
#include <online_cpp/code/protocol.h>
#include <online_cpp/code/setup.h>

#include <nlohmann/json.hpp>

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
  std::ifstream f("gods_cpp/cards.json");
  if (!f) {
    std::cerr << "Could not open gods_cpp/cards.json\n";
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

// Replicates gods/setup.py:quick_setup → create_draft.
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

// Build the initial Table_State, aligning table_state.cards 1:1 with
// gods_state.all_cards.
static Table_State init_table_state(
  Game_State& gods_state, UI_State& ui_state, int bottom_player = 0
) {
  Table_State table_state;

  auto draw_power = [&gods_state, &ui_state, &table_state](Thing& thing) {
    const KT_Card& card  = static_cast<const KT_Card&>(thing);
    const auto&    gcard = gods_state.all_cards[card.id];
    std::string    power = std::to_string(gods_state.effective_power(card.id));
    draw_card_power_badge(power, gcard.destroyed);

    // Highlight ring for cards the agent is asking us to choose from.
    int w = kt::CARD_WIDTH;
    int h = kt::CARD_HEIGHT;
    for (const auto& [k, kt_card_id] : ui_state.highlighted_cards) {
      if (kt_card_id == card.id) {
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

  // Cards aligned with all_cards so card.id is the shared key.
  for (const auto& gc : gods_state.all_cards) {
    KT_Card kc;
    kc.id            = gc.id;
    kc.image_path    = get_image_path(card_designs[gc.id]->name);
    kc.draw_callback = draw_power;
    table_state.cards.push_back(kc);
  }

  // Layout + initial assignment of cards to stacks.
  std::vector<Stack>         stacks = make_gods_stacks(bottom_player);
  std::map<std::string, int> name_to_idx;
  for (int i = 0; i < (int)stacks.size(); ++i) name_to_idx[stacks[i].name] = i;

  for (int i = 0; i < 2; ++i) {
    const Player& p = gods_state.players[i];
    stacks[name_to_idx["p" + std::to_string(i) + "_deck"]].cards    = p.deck;
    stacks[name_to_idx["p" + std::to_string(i) + "_hand"]].cards    = p.hand;
    stacks[name_to_idx["p" + std::to_string(i) + "_discard"]].cards = p.discard;
    stacks[name_to_idx["p" + std::to_string(i) + "_wonders"]].cards = p.wonders;
    std::vector<int> peoples;
    for (int pid : gods_state.peoples) {
      if (gods_state.owner(pid) == i) peoples.push_back(pid);
    }
    stacks[name_to_idx["p" + std::to_string(i) + "_peoples"]].cards = peoples;
  }
  stacks[name_to_idx["shared_deck"]].cards = gods_state.shared_deck;

  table_state.stacks = stacks;
  for (Stack& s : table_state.stacks) {
    update_card_positions(s, table_state, /*sort=*/false);
  }
  return table_state;
}

// Per-frame HUD overlay drawn by draw_table via Table_State::draw_callback.
static void draw_hud(
  Table_State*                 table_state,
  Game_State&                  gods_state,
  const std::optional<Choice>& current_choice,
  UI_State&                    ui_state,
  int                          bottom_player
) {
  int       H      = kt::WINDOW_HEIGHT;
  int       h      = kt::CARD_HEIGHT;
  int       margin = 20;
  Rectangle window = {0.0f, 0.0f, (float)kt::WINDOW_WIDTH, (float)H};
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

  ui_state.draw_buttons();

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
  if (immediate_button(button, label, Color{20, 20, 20, 100})) {
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
    const KT_Card& kt_card = table_state->animated_cards[card_id];
    int            btn_w = 44, btn_h = 36, gap = 6;
    int            panel_w   = 10 * btn_w + 9 * gap + 16;
    Rectangle      card_rect = {
      kt_card.x, kt_card.y, (float)kt::CARD_WIDTH, (float)kt::CARD_HEIGHT
    };
    Rectangle panel =
      place_next(card_rect, panel_w, btn_h + 16, "center", "bottom", 8);
    panel.x =
      std::max(0.0f, std::min(panel.x, (float)(kt::WINDOW_WIDTH - panel_w)));
    panel.y = std::max(
      0.0f, std::min(panel.y, (float)(kt::WINDOW_HEIGHT - (int)panel.height))
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
      if (immediate_button(btn, std::to_string(v), col)) {
        gods_state.all_cards[card_id].power = v;
        ui_state.power_edit_card_id         = -1;
        gods_state.notify_cards_changed();
      }
      btn.x += (float)(btn_w + gap);
    }
  }

  // Re-place the shared deck so it stays anchored.
  for (Stack& s : table_state->stacks) {
    if (s.name == "shared_deck") {
      Rectangle target =
        ui_state.playground
          ? place_inside(
              window, kt::CARD_WIDTH, kt::CARD_HEIGHT, "right", "center", 10
            )
          : place_next(
              window, kt::CARD_WIDTH, kt::CARD_HEIGHT, "right", "center", 10
            );
      s.rect = target;
      update_card_positions(s, *table_state, false);
      break;
    }
  }
}

// The main game loop, mirrors play_gods() from main.py.
static void play_gods(
  Game_State&                 gods_state,
  Table_State&                table_state,
  UI_State&                   ui_state,
  Agent*                      agent,
  int                         player_index,
  UDP_Socket*                 sock,
  std::pair<std::string, int> friend_addr
) {
  if (!IsWindowReady()) {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(kt::WINDOW_WIDTH, kt::WINDOW_HEIGHT, "Gods Online");
    SetTargetFPS(kt::TARGET_FPS);
  }

  std::optional<Choice> current_choice;
  bool                  prev_playground = ui_state.playground;

  table_state.draw_callback = [&](Table_State* ts) {
    draw_hud(ts, gods_state, current_choice, ui_state, player_index);
  };

  // Online: broadcast card state on every notify_cards_changed.
  if (sock) {
    gods_state.on_cards_changed = [&]() {
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
  }

  while (!WindowShouldClose()) {
    if (gods_state.game_over) break;

    // SPACE-to-zoom: handled by kitchen_table's update_input, but our outer
    // loop also peeks SPACE for the same gesture (to keep parity with Python).
    if (IsKeyDown(KEY_SPACE)) {
      auto r =
        find_card_at((float)GetMouseX(), (float)GetMouseY(), table_state);
      table_state.zoomed_card_id = r ? r->first : -1;
    } else {
      table_state.zoomed_card_id = -1;
    }

    update_input(table_state);
    int mx = GetMouseX(), my = GetMouseY();

    // Playground: P opens the power editor for the hovered card.
    if (ui_state.playground && IsKeyPressed(KEY_P)) {
      auto r = find_card_at((float)mx, (float)my, table_state);
      if (r) {
        int hovered = r->first;
        ui_state.power_edit_card_id =
          (ui_state.power_edit_card_id == hovered) ? -1 : hovered;
      } else {
        ui_state.power_edit_card_id = -1;
      }
    }

    // While power editor is open, 1-9 / 0 set the power directly.
    if (ui_state.power_edit_card_id != -1) {
      int keys[10] = {
        KEY_ONE,
        KEY_TWO,
        KEY_THREE,
        KEY_FOUR,
        KEY_FIVE,
        KEY_SIX,
        KEY_SEVEN,
        KEY_EIGHT,
        KEY_NINE,
        KEY_ZERO
      };
      for (int i = 0; i < 10; ++i) {
        if (IsKeyPressed(keys[i])) {
          gods_state.all_cards[ui_state.power_edit_card_id].power =
            (i < 9) ? (i + 1) : 10;
          ui_state.power_edit_card_id = -1;
          gods_state.notify_cards_changed();
          break;
        }
      }
    }

    // Click-to-expand for discard stacks.
    int discard_you      = -1;
    int discard_opponent = -1;
    for (int i = 0; i < (int)table_state.stacks.size(); ++i) {
      if (table_state.stacks[i].name ==
          "p" + std::to_string(player_index) + "_discard")
        discard_you = i;
      if (table_state.stacks[i].name ==
          "p" + std::to_string(1 - player_index) + "_discard")
        discard_opponent = i;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      for (int stack_id : {discard_opponent, discard_you}) {
        if (stack_id < 0) continue;
        Stack& s           = table_state.stacks[stack_id];
        bool   is_expanded = s.spread_x > 0.0f;
        bool   inside      = point_in_stack_area((float)mx, (float)my, s);
        if (inside && !is_expanded) {
          s.rect = ui_state.place(
            kt::CARD_WIDTH * 7, kt::CARD_HEIGHT, "center", "center"
          );
          s.spread_x = 150.0f;
          s.depth    = 1.0f;
          update_card_positions(s, table_state, false);
        } else if (is_expanded && !inside) {
          // Restore original rect from a fresh layout.
          auto restore_rect = make_gods_stacks(player_index)[stack_id].rect;
          s.rect            = restore_rect;
          s.spread_x        = 0.0f;
          s.depth           = 0.0f;
          update_card_positions(s, table_state, false);
        }
      }
    }

    // Sync playground mode with remote player.
    if (sock && ui_state.playground != prev_playground) {
      nlohmann::json msg;
      msg["type"] = "playground";
      msg["on"]   = ui_state.playground;
      send_message(*sock, msg, friend_addr);
      if (ui_state.playground) {
        nlohmann::json stacks = nlohmann::json::array();
        for (const Stack& s : table_state.stacks) stacks.push_back(s.cards);
        nlohmann::json sm;
        sm["type"]   = "stacks";
        sm["stacks"] = stacks;
        send_message(*sock, sm, friend_addr);
      }
    }
    prev_playground = ui_state.playground;

    // Send stacks if in playground/no-logic mode and something changed.
    if ((!agent || ui_state.playground) && sock) {
      auto dropped     = table_state.poll_dropped_card();
      bool should_send = dropped.has_value() || IsKeyPressed(KEY_R) ||
                         IsKeyPressed(KEY_S);
      if (should_send) {
        nlohmann::json stacks = nlohmann::json::array();
        for (const Stack& s : table_state.stacks) stacks.push_back(s.cards);
        nlohmann::json sm;
        sm["type"]   = "stacks";
        sm["stacks"] = stacks;
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
          const auto& arr = msg["stacks"];
          for (size_t i = 0; i < arr.size() && i < table_state.stacks.size();
               ++i) {
            std::vector<int> cards      = arr[i].get<std::vector<int>>();
            table_state.stacks[i].cards = cards;
            update_card_positions(table_state.stacks[i], table_state, false);
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
    draw_background(turn);
    draw_table(table_state);

    if (agent && !ui_state.playground) {
      current_choice = game_frame(gods_state, *agent, current_choice);
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
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "agent")
      skip_menu_vs_ai = true;
    else if (a.rfind("--seed=", 0) == 0)
      seed = std::atoi(a.c_str() + 7);
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

  Game_State  gods_state = quick_setup(seed);
  UI_State    ui_state;
  Table_State table_state =
    init_table_state(gods_state, ui_state, player_index);

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
  play_gods(
    gods_state, table_state, ui_state, &duel, player_index, sock, friend_addr
  );

  return 0;
}
