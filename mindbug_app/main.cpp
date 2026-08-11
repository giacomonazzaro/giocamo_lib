#include <game/agent.h>
#include <game/game.h>
#include <game/minimax.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <mindbug/gameplay.h>
#include <mindbug/models.h>
#include <struct/json.h>  // for save_to_json()
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <iostream>
#include <vector>

#include "agent_ui.h"
#include "ui.h"

// On desktop the art sits relative to the working directory; on web it is
// preloaded at an absolute path in MEMFS, and the emscripten working directory
// isn't guaranteed to be "/".
#ifdef __EMSCRIPTEN__
static const std::string IMAGES_DIR = "/mindbug/card-images";
#else
static const std::string IMAGES_DIR = "mindbug/card-images";
#endif

std::string get_image_path(const std::string& image_file);

std::vector<Thing> make_mindbug_zones(
  int bottom_player, int window_width, int window_height
);

// Where the debug snapshot of the game is written and read.
static const std::string SNAPSHOT_PATH = "data/debug_game_state.json";

// Diameter of the life counter.
static constexpr int LIFE_COUNTER_SIZE = 90;

// Mindbug on the table. The table is laid out once here; play_game deals the
// game and drives the loop through these hooks.
struct Mindbug_Giocamo : Giocamo {
  bool show_opponent_hand;

  Mindbug_Giocamo(mindbug::Game_State& game, Mindbug_Agent_UI& agent_ui)
      : Giocamo(game, agent_ui) {}

  void init_table() override {
    auto bottom_player = this->bottom_player;
    auto hot_seat      = this->hot_seat;

    table.is_drop_allowed = [](int, int, int) { return false; };

    // One Thing per card of the deal; ids match the game's card indices. The
    // deal comes later, so a card takes its art in update_table_from_game.
    const int card_count = 2 * (mindbug::HAND_SIZE + mindbug::DRAW_PILE_SIZE);
    for (int card = 0; card < card_count; ++card) {
      table.things.push_back(make_card());
      table.draw_callbacks[card] =
        make_card_draw_callback(this->mindbug_game(), card);
    }

    // The Mindbugs are cards too, two per player, right after the deal.
    for (int player = 0; player < 2; ++player) {
      for (int i = 0; i < mindbug::STARTING_MINDBUGS; ++i) {
        table.things.push_back(make_card(get_image_path("mindbug.png")));
      }
    }

    // Life is a counter per player: the Thing draws its own value.
    for (int player = 0; player < 2; ++player) {
      auto counter    = Thing();
      counter.shape   = circle_shape(LIFE_COUNTER_SIZE);
      counter.color   = {190, 40, 45, 255};
      counter.counter = {mindbug::STARTING_LIFE, 0, 20};
      table.things.push_back(std::move(counter));
    }

    auto zone_ids = std::vector<int>();
    auto zones =
      make_mindbug_zones(bottom_player, tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT);
    for (Thing& zone : zones) {
      zone_ids.push_back(add_thing(table, std::move(zone)));
    }

    // Empty texture path: the table is drawn with root.color.
    auto root      = create_table_root(tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "");
    root._children = zone_ids;
    root.color     = {0, 0, 0, 0};
    table.root     = add_thing(table, std::move(root));

    table.draw_callbacks[-1] = [this](const Table_State&, const Input&, bool) {
      draw_mindbug_hud(this->mindbug_game(), this->bottom_player);
    };
  }

  // The Thing holding player's Mindbug number `index`. They follow the dealt
  // cards, so their ids are fixed by the deal size.
  static int mindbug_thing(int player, int index) {
    return 2 * (mindbug::HAND_SIZE + mindbug::DRAW_PILE_SIZE) +
           player * mindbug::STARTING_MINDBUGS + index;
  }

  // The counter Thing holding a player's life. The two follow the Mindbugs.
  static int life_thing(int player) {
    return mindbug_thing(1, mindbug::STARTING_MINDBUGS) + player;
  }

  mindbug::Game_State& mindbug_game() {
    return static_cast<mindbug::Game_State&>(game);
  }

  Mindbug_Agent_UI& mindbug_agent_ui() {
    return static_cast<Mindbug_Agent_UI&>(agent_ui);
  }

  // Every zone owns the cards the game says it holds, and a hand is only face
  // up for the player it belongs to.
  void update_table_from_game() override {
    mindbug::Game_State& state = this->mindbug_game();

    // Clicking a card also starts dragging it, and the card the player just
    // clicked is about to change zone. End the drag first, or the layout would
    // look for it in the zone it has already left.
    table.drag_state = Drag_State();

    for (int card = 0; card < state.all_cards.size(); ++card) {
      const mindbug::Card_Design& design =
        mindbug::card_designs[mindbug::design_of(state, card)];
      table.things[card].image_path = get_image_path(design.image);
    }

    auto set_zone =
      [&](const std::string& name, const std::vector<int>& cards) {
        const int zone               = find_thing(table, name);
        table.things[zone]._children = cards;
        update_children_positions(zone, table, false);
      };

    for (int player = 0; player < 2; ++player) {
      const std::string      prefix = "p" + std::to_string(player) + "_";
      const mindbug::Player& seat   = state.players[player];
      set_zone(prefix + "hand", {seat.hand.begin(), seat.hand.end()});
      set_zone(prefix + "draw", {seat.draw_pile.begin(), seat.draw_pile.end()});
      set_zone(prefix + "discard", {seat.discard.begin(), seat.discard.end()});
      set_zone(
        prefix + "creatures", {seat.creatures.begin(), seat.creatures.end()}
      );

      // A spent Mindbug is turned face down and stays on the table.
      auto mindbugs = std::vector<int>();
      for (int i = 0; i < mindbug::STARTING_MINDBUGS; ++i) {
        const int thing             = mindbug_thing(player, i);
        table.things[thing].face_up = i < seat.mindbugs;
        mindbugs.push_back(thing);
      }
      set_zone(prefix + "mindbugs", mindbugs);
      table.things[life_thing(player)].counter.value = seat.life;
      set_zone(prefix + "life", {life_thing(player)});

      // You always see your own hand; the opponent's is face down unless both
      // players share this screen.
      const bool visible = player == bottom_player || show_opponent_hand;
      table.things[find_thing(table, prefix + "hand")].face_up = visible;
    }

    // The creature waiting on a Mindbug decision is face up for both players —
    // the opponent has to see what they may take.
    set_zone(
      "played",
      state.played_card == -1 ? std::vector<int>{}
                              : std::vector<int>{state.played_card}
    );

    save_to_json<mindbug::Game_State>(mindbug_game(), SNAPSHOT_PATH);
    save_to_json<Table_Layout>(table, "data/debug_table_state.json");
    printf("Saved debug snapshot to data/debug_*.json\n");
  }

  // --load: carry on from the snapshot. The pending choice is worked out again
  // from the phase that was saved; effects that still owed a decision are not
  // in the snapshot, so those are lost.
  bool load_game(const std::string& path) override {
    try {
      mindbug_game() = load_from_json<mindbug::Game_State>(
        path.empty() ? SNAPSHOT_PATH : path
      );
    } catch (const std::exception& error) {
      std::cerr << error.what() << "\n";
      return false;
    }
    game.begin_game();
    return true;
  }

  Agent* agent_opponent() override {
    return new Agent_Minimax_Stochastic<mindbug::Game_State>(
      /* max_depth   */ 13,
      /* num_samples */ 15
    );
  }

  std::vector<int> player_scores() override {
    return {
      mindbug::compute_player_score(this->mindbug_game(), 0),
      mindbug::compute_player_score(this->mindbug_game(), 1),
    };
  }
};

std::vector<Thing> make_mindbug_zones(
  int bottom_player, int window_width, int window_height
) {
  const int card_width  = tt::CARD_WIDTH;
  const int card_height = tt::CARD_HEIGHT;
  const int margin      = 24;
  const int fan         = tt::CARD_WIDTH + 10;  // Spread of a row of cards.
  const int pile        = -3;                   // Spread of a near-flat pile.

  // const int row_width   = 6 * fan + card_width;
  const int row_width = 5 * tt::CARD_WIDTH + 10;

  // The root is centered on the screen, so the window spans
  // (-width/2, -height/2) to (width/2, height/2) in root-local coordinates.
  Rectangle window = {
    -(float)window_width / 2.0f,
    -(float)window_height / 2.0f,
    (float)window_width,
    (float)window_height
  };

  // Bottom seat: hand along the bottom edge, creatures in the half above it,
  // draw pile and discard pile out on the flanks.
  Rectangle hand =
    place_inside(window, row_width, card_height, "center", "bottom", margin);
  Rectangle creatures =
    place_next(hand, row_width, card_height, "center", "top", margin);
  Rectangle draw_pile =
    place_next(hand, card_width, card_height, "left", "center", margin);
  Rectangle discard =
    place_next(hand, card_width, card_height, "right", "center", margin);
  // The two Mindbugs sit out on the flank, beside the creatures, and the life
  // counter on the flank opposite.
  Rectangle mindbugs = place_next(
    creatures, card_width + 90, card_height, "left", "center", margin
  );
  Rectangle life = place_next(
    draw_pile, LIFE_COUNTER_SIZE, LIFE_COUNTER_SIZE, "left", "center", margin
  );

  // The opponent's zones mirror them across the middle of the screen.
  auto mirrored = [](Rectangle r) -> Rectangle {
    return {r.x, -r.y - r.height, r.width, r.height};
  };

  const int top_player = 1 - bottom_player;
  auto      zone_name  = [](int seat, const char* zone) {
    return "p" + std::to_string(seat) + "_" + zone;
  };

  // A draw pile is face down for both players; everything else is open.
  std::vector<Thing> zones;
  for (int p : {bottom_player, top_player}) {
    if (p == top_player) {
      hand      = mirrored(hand);
      creatures = mirrored(creatures);
      draw_pile = mirrored(draw_pile);
      discard   = mirrored(discard);
      mindbugs  = mirrored(mindbugs);
      life      = mirrored(life);
    }

    zones.push_back(
      make_container_thing(hand, fan, 0, true, zone_name(p, "hand"))
    );
    zones.push_back(
      make_container_thing(creatures, fan, 0, true, zone_name(p, "creatures"))
    );
    zones.push_back(
      make_container_thing(draw_pile, 0, pile, false, zone_name(p, "draw"))
    );
    zones.push_back(make_container_thing(
      discard, 0, p == bottom_player ? 35 : -35, true, zone_name(p, "discard")
    ));
    zones.push_back(
      make_container_thing(mindbugs, 90, 0, true, zone_name(p, "mindbugs"))
    );
    zones.push_back(
      make_container_thing(life, 0, 0, true, zone_name(p, "life"))
    );
  }

  // The creature waiting on a Mindbug decision.
  Rectangle played =
    place_inside(window, card_width, card_height, "center", "center");
  played.x += 200;
  zones.push_back(make_container_thing(played, 0, pile, true, "played"));

  return zones;
}

std::string get_image_path(const std::string& image_file) {
  if (image_file.empty()) return "";
  return IMAGES_DIR + "/" + image_file;
}

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  if (!mindbug::load_card_designs()) {
    std::cerr << "run mindbug_app from the repository root\n";
    return 1;
  }

  auto game     = mindbug::Game_State();
  auto agent_ui = Mindbug_Agent_UI();
  auto giocamo  = Mindbug_Giocamo(game, agent_ui);

  // Agent* agent = make_agent_pair(
  //   &agent_ui, giocamo.agent_opponent(), menu_result, options.vs_ai
  // );

  play_game(giocamo, options, "Mindbug");
  return 0;
}
