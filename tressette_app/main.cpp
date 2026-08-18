#include <game/agent.h>
#include <game/game.h>
#include <game/mcts.h>
#include <giocamo/menu.h>
#include <giocamo/play.h>
#include <tabletop/config.h>
#include <tabletop/input_recorder.h>
#include <tabletop/rendering.h>
#include <tabletop/tabletop.h>
#include <tabletop/ui.h>
#include <tressette/ai.h>
#include <tressette/gameplay.h>
#include <tressette/models.h>
#include <tressette/neural_agent.h>

// raylib last: its color macros (RED/GREEN/BLUE) would expand inside enums
// otherwise.
#include <raylib.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include <struct/imgui.h>  // for draw_editor_ui()
#include <struct/json.h>   // for to_json()

#include "agent_ui.h"
#include "ui.h"

// MCTS with softmax-weighted (guided) rollouts: instead of playing the rollout
// uniformly at random, each step is biased toward stronger moves scored by
// evaluate_state. This gives a far better value signal than random rollouts,
// which is what limits plain MCTS in Tressette (a random rollout opponent never
// punishes bad play, so e.g. leading an Ace looks safe when it isn't).
static Agent* make_softmax_mcts(
  int num_iterations, int rollout_depth, int num_samples, float time_budget
) {
  using Game_State = tressette::Game_State;
  auto* agent =
    new Agent_MCTS_Stochastic<Game_State, Agent_Softmax_Rollout<Game_State>>(
      num_samples, num_iterations, rollout_depth, 1.41421356f, time_budget
    );
  // Each search builds its own rollout agent; lower temperature is greedier
  // (sharper guidance), higher is closer to random.
  for (auto& search : agent->agents) {
    search.rollout_agent_factory = []() {
      return Agent_Softmax_Rollout<Game_State>(/* temperature */ 0.5f);
    };
  }
  return agent;
}

// Tressette on the table. The table is laid out once here; play_game deals the
// game and drives the loop through these hooks.
struct Tressette_Giocamo : Giocamo_With_History<tressette::Game_State> {
  Tressette_Giocamo(tressette::Game_State& game, Tressette_Agent_UI& agent_ui)
      : Giocamo_With_History<tressette::Game_State>(game, agent_ui) {}

  tressette::Game_State& tressette_game() {
    return static_cast<tressette::Game_State&>(game);
  }
  const tressette::Game_State& tressette_game() const {
    return static_cast<const tressette::Game_State&>(game);
  }

  Tressette_Agent_UI& tressette_agent_ui() {
    return static_cast<Tressette_Agent_UI&>(agent_ui);
  }

  void init_table() override {
    tressette::Game_State& state  = this->tressette_game();
    Tressette_Agent_UI&    player = this->tressette_agent_ui();
    player.player_index           = bottom_player;
    table.is_drop_allowed         = [](int, int, int) { return false; };

    // The acknowledge pause is owned by the seat this screen plays.
    state.human_player = bottom_player;

    // One Thing per card; ids 0..39 match all_cards indices.
    for (const auto& card : tressette::all_cards) {
      auto thing = make_card();
      if (card.suit == tressette::Suit::COPPE) thing.color = {50, 100, 50, 255};
      if (card.suit == tressette::Suit::DENARI)
        thing.color = {150, 120, 20, 255};
      if (card.suit == tressette::Suit::BASTONI) thing.color = {80, 50, 50, 255};
      if (card.suit == tressette::Suit::SPADE) thing.color = {70, 80, 150, 255};

      table.things.push_back(thing);
      table.draw_callbacks[card.id] =
        make_card_draw_callback(state, player.ui_state, card.id);
    }

    // 6 stack Things appended after cards. The opponent's hand is face up only
    // in hot-seat, where one screen is shared.
    std::vector<Thing> stacks =
      make_tressette_stacks(bottom_player, hot_seat);
    auto stack_ids = std::vector<int>();
    for (Thing& stack : stacks) {
      stack_ids.push_back(add_thing(table, std::move(stack)));
    }

    // Root: a wooden table surface owning all stacks as direct children.
    auto root = create_table_root(
      tt::WINDOW_WIDTH, tt::WINDOW_HEIGHT, "tabletop/data/wood.png"
    );
    root._children = stack_ids;
    table.root     = add_thing(table, std::move(root));

    // Per-player HUD overlay. Drawn on top of every frame via the -1 callback.
    table.draw_callbacks[-1] = [this](const Table_State&, const Input&, bool) {
      const tressette::Game_State& state = this->tressette_game();
      for (int i = 0; i < 2; ++i) {
        int  score      = tressette::compute_player_score(state, i);
        bool is_current = (i == state.current_player);
        int  hud_y = (i == this->bottom_player) ? (tt::WINDOW_HEIGHT - 56) : 16;
        draw_tressette_player_hud(i, score, is_current, hud_y);
      }
    };
  }

  void update_table_from_game() override {
    tressette::Game_State& state = this->tressette_game();

    auto refresh = [&](const char* name, array<const int> cards) {
      table.things[find_thing(table, name)]._children.assign(
        cards.data, cards.data + cards.size()
      );
    };
    refresh("p0_hand", state.players[0].hand);
    refresh("p1_hand", state.players[1].hand);
    refresh("p0_tricks", state.players[0].tricks_won);
    refresh("p1_tricks", state.players[1].tricks_won);
    refresh("stock", state.stock);
    refresh("table", state.trick);

    // On the first call the table has not been drawn yet, so there are no
    // world transforms to read a card's current place from.
    const bool first_layout = table.world_transforms.size() !=
                              table.things.size();
    if (!first_layout) update_local_transforms_to_match_world_transforms(table);
    for (size_t i = 0; i < table.things.size(); i++) {
      update_children_positions(i, table, !first_layout);
    }
  }

  // Leaving playground: the table is what the player arranged, so read it back
  // into the game. Card Things carry the game's card indices, so a stack's
  // children are the cards of that stack.
  void update_game_from_table() override {
    table.is_drop_allowed        = [](int, int, int) { return false; };
    tressette::Game_State& state = this->tressette_game();

    auto stack_cards = [&](const char* name) -> const std::vector<int>& {
      return table.things[find_thing(table, name)].children();
    };
    auto assign = [](auto& target, const std::vector<int>& cards) {
      target.clear();
      for (int card : cards) target.push_back(card);
    };
    assign(state.players[0].hand, stack_cards("p0_hand"));
    assign(state.players[1].hand, stack_cards("p1_hand"));
    assign(state.players[0].tricks_won, stack_cards("p0_tricks"));
    assign(state.players[1].tricks_won, stack_cards("p1_tricks"));
    assign(state.stock, stack_cards("stock"));
    assign(state.trick, stack_cards("table"));
  }

  Agent* agent_opponent() override {
#ifdef TORCH_AVAILABLE
    return new tressette::Agent_Minimax_Neural(
      "tressette/tressette_value_traced.pt", 3, 20
    );
#elif defined(__EMSCRIPTEN__)
    // Web: the search runs one determinization per frame so the page stays
    // responsive (see the Emscripten branch of Agent_MCTS_Stochastic). Guided
    // rollouts cost more per iteration, so keep num_iterations modest.
    return make_softmax_mcts(
      /* num_iterations */ 5000,
      /* rollout_depth */ 40,
      /* num_samples */ 40,
      /* time_budget_seconds */ 0.0f
    );
#else
    // Guided rollouts are several times slower per iteration, but plain MCTS
    // already saturates after a few hundred iterations here, so a smaller
    // iteration cap with a better rollout policy spends the time budget far
    // better than brute-forcing random rollouts.
    return make_softmax_mcts(
      /* num_iterations */ 20000,
      /* rollout_depth */ 40,
      /* num_samples */ 50,
      /* time_budget_seconds */ 5.0f
    );
#endif
  }

  std::vector<int> player_scores() const override {
    return {
      tressette::compute_player_score(this->tressette_game(), 0),
      tressette::compute_player_score(this->tressette_game(), 1),
    };
  }
};

int main(int argc, char** argv) {
  auto options = parse_play_args(argc, argv);

  auto game     = tressette::Game_State();
  auto agent_ui = Tressette_Agent_UI();
  auto giocamo  = Tressette_Giocamo(game, agent_ui);

  play_game(giocamo, options, "Tressette");
  return 0;
}
