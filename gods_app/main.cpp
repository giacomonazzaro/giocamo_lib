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
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

#include <nlohmann/json.hpp>

#include "../struct/imgui.h"
#include "../struct/json.h"
#include "../tabletop/tabletop_json.h"
#include "agent_ui.h"
#include "ui.h"

// raylib last; its color-name macros (RED/GREEN/BLUE/...) would otherwise
// expand inside Card_Color and break the enum.
#include <raylib.h>

namespace fs_helpers {

// Art file of each card, indexed like card_designs.
static std::vector<std::string> card_images;

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
  card_images.clear();
  for (size_t i = 0; i < data.size(); ++i) {
    const auto& d = data[i];
    entries.emplace_back(
      d.value("name", ""),
      d.value("type", ""),
      d.value("color", ""),
      d.value("effect", "")
    );
    card_images.push_back(d.value("image", ""));
  }
  set_card_designs(entries);
}

}  // namespace fs_helpers

// Push gods_state's deck/hand/discard/peoples/wonders/shared_deck into the
// matching stacks (looked up by name), then refresh each stack's card
// positions. Reused after both fresh layout init and JSON load to keep the
// scene tree consistent with the current Game_State.
void populate_stacks_from_gods_state(
  Table_State& table_state, Game_State& gods_state
) {
  auto set_children = [&](const std::string& name, array<const int> ids) {
    table_state.things[find_thing(table_state, name)]._children.assign(
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
    auto  image_path = get_image_path(fs_helpers::card_images[gc.id]);
    Thing card       = make_card(image_path);
    table_state.things.push_back(card);
  }

  // Stack things: assign ids by append order. Track the insertion order so
  // root.children() matches the original stack ordering. make_gods_stacks
  // returns stacks in root-local coords (root is centered on the screen).
  std::vector<Thing> stacks =
    make_gods_stacks(bottom_player, window_width, window_height);
  std::vector<int> stack_ids_in_order;
  for (Thing& s : stacks) {
    stack_ids_in_order.push_back(add_thing(table_state, std::move(s)));
  }

  // Root: sits at the end of `things`, owns all stacks as direct children.
  // Centered on the screen so the root rect spans (0,0)-(W,H) in world.
  Thing root;
  root.name  = "root";
  root.shape = rectangle_shape({(float)window_width, (float)window_height});
  root.transform.x = (float)window_width / 2.0f;
  root.transform.y = (float)window_height / 2.0f;
  root._children   = stack_ids_in_order;
  root.capacity    = 0;
  // Transparent so the shader background drawn behind the table shows through.
  root.color = {0, 0, 0, 0};
  table_state.root = add_thing(table_state, std::move(root));

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
  sync_game_state_from_table(table_state, gods_state);
  save_to_json<Game_State>(gods_state, "data/debug_game_state.json");
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

// Gods on the table. The table is laid out once here; play_game deals the game
// and drives the loop through these hooks.
struct Gods_Giocamo : Giocamo_With_History<Game_State> {
  Gods_Giocamo(Game_State& game, Gods_Agent_UI& agent_ui)
      : Giocamo_With_History<Game_State>(game, agent_ui) {}

  Game_State& gods_game() { return static_cast<Game_State&>(game); }
  const Game_State& gods_game() const {
    return static_cast<const Game_State&>(game);
  }

  Gods_Agent_UI& gods_agent_ui() {
    return static_cast<Gods_Agent_UI&>(agent_ui);
  }

  void init_table() override {
    Game_State&    state  = this->gods_game();
    Gods_Agent_UI& player = this->gods_agent_ui();
    player.bottom_player  = bottom_player;

    init_table_layout(
      table, state, bottom_player, tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT
    );
    init_card_draw_callbacks(table, state, player.ui_state);

    // Per-frame overlay: gods-specific inputs (debug save, discard expand)
    // plus the HUD drawing. The -1 callback runs every frame with the current
    // input, so all the per-frame gods logic lives here.
    table.draw_callbacks[-1] =
      [this](const Table_State&, const Input& input, bool) {
        Game_State&    state  = this->gods_game();
        Gods_Agent_UI& player = this->gods_agent_ui();
        handle_debug_save(table, state, input);
        handle_discard_expand(
          table, player.ui_state, this->bottom_player, input
        );
        draw_hud(&table, state, player.ui_state, this->bottom_player, input);
      };
  }

  void update_table_from_game() override {
    update_stacks(table, this->gods_game());
  }

  // Leaving playground: read the rearranged table back into the game so play
  // resumes from the user's layout.
  void update_game_from_table() override {
    Gods_Agent_UI& player = this->gods_agent_ui();
    sync_game_state_from_table(table, this->gods_game());
    player.ui_state.power_edit_card_id = -1;
    player.ui_state.playground         = false;
  }

  // Playground only: P opens the power editor for the hovered card, 1-9 / 0
  // set the power. draw_hud draws the panel for the card being edited.
  bool draw_game_editor() override {
    Gods_Agent_UI& player      = this->gods_agent_ui();
    player.ui_state.playground = true;
    const Input& input         = *this->agent_ui.input;
    return handle_power_editor(this->gods_game(), table, player.ui_state, input);
  }

  Agent* agent_opponent() override {
    return new Agent_Minimax_Stochastic_Gods(6, 20);
  }

  std::vector<int> player_scores() const override {
    Game_State copy = this->gods_game();  // The score is computed in place.
    return {compute_player_score(copy, 0), compute_player_score(copy, 1)};
  }
};

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);
  // "agent" is the old spelling of --skip-menu.
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "agent") options.skip_menu = true;
  }

  // Card hooks (Card::on_played etc.) dispatch through the global card_designs
  // registry, so it must be populated before any gameplay runs.
  fs_helpers::load_card_designs();

  auto game     = Game_State();
  auto agent_ui = Gods_Agent_UI();
  auto giocamo  = Gods_Giocamo(game, agent_ui);

  play_game(giocamo, options, "Gods");
  return 0;
}
