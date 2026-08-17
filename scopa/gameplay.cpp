#include "gameplay.h"

#include <algorithm>
#include <random>
#include <string>

namespace scopa {

// Remove a single occurrence of value from v. No-op if not present.
static void erase_value(std::vector<int>& v, int value) {
  auto it = std::find(v.begin(), v.end(), value);
  if (it != v.end()) v.erase(it);
}

// Recursive enumeration of every subset of `table_card_ids` whose ranks sum
// exactly to `target`. Each subset is appended to `output` as a fresh
// vector of card ids. Order within a subset matches the table order.
static void enumerate_sum_subsets(
  const Game_State&              state,
  const std::vector<int>&        table_card_ids,
  int                            start,
  int                            target,
  std::vector<int>&              current,
  std::vector<std::vector<int>>& output
) {
  if (target == 0 && !current.empty()) {
    output.push_back(current);
    return;
  }
  for (int i = start; i < (int)table_card_ids.size(); ++i) {
    int rank = state.all_cards[table_card_ids[i]].rank;
    if (rank > target) continue;
    current.push_back(table_card_ids[i]);
    enumerate_sum_subsets(
      state, table_card_ids, i + 1, target - rank, current, output
    );
    current.pop_back();
  }
}

std::vector<Action> enumerate_actions(const Game_State& state) {
  std::vector<Action> actions;
  const Player&       player = state.players[state.current_player];
  for (int played_card_id : player.hand) {
    const int played_rank = state.all_cards[played_card_id].rank;

    // Exact-match cards on the table take priority over sum captures. If at
    // least one exact match exists, the player must take exactly one (and
    // can choose which one if there are duplicates).
    std::vector<int> exact_matches;
    for (int table_card_id : state.table) {
      if (state.all_cards[table_card_id].rank == played_rank)
        exact_matches.push_back(table_card_id);
    }
    if (!exact_matches.empty()) {
      for (int table_card_id : exact_matches) {
        Action action;
        action.played_card_id    = played_card_id;
        action.captured_card_ids = {table_card_id};
        actions.push_back(action);
      }
      continue;
    }

    // No exact match: enumerate every subset of the table that sums to the
    // played rank.
    std::vector<std::vector<int>> sum_subsets;
    std::vector<int>              current_subset;
    enumerate_sum_subsets(
      state, state.table, 0, played_rank, current_subset, sum_subsets
    );
    if (sum_subsets.empty()) {
      // No capture is possible: the card stays on the table.
      Action action;
      action.played_card_id = played_card_id;
      actions.push_back(action);
    } else {
      for (auto& subset : sum_subsets) {
        Action action;
        action.played_card_id    = played_card_id;
        action.captured_card_ids = std::move(subset);
        actions.push_back(action);
      }
    }
  }
  return actions;
}

void sort_hand(Game_State& state, int player_index) {
  auto& hand = state.players[player_index].hand;
  std::sort(hand.begin(), hand.end(), [&state](int a, int b) {
    const Card& ca = state.all_cards[a];
    const Card& cb = state.all_cards[b];
    if (ca.suit != cb.suit) return ca.suit < cb.suit;
    return ca.rank < cb.rank;
  });
}

// Deal up to 9 cards from the stock to each player. Used after the initial
// 9-card hand has been spent so the second batch of cards comes from a
// known source rather than a face-down mid-round draw.
static void deal_second_hand(Game_State& state) {
  for (int player_index = 0; player_index < 2; ++player_index) {
    Player& player = state.players[player_index];
    while ((int)player.hand.size() < 9 && !state.stock.empty()) {
      player.hand.push_back(state.stock.back());
      state.stock.pop_back();
    }
    sort_hand(state, player_index);
  }
}

void apply_action(Game_State& state, const Action& action) {
  Player& player = state.players[state.current_player];
  erase_value(player.hand, action.played_card_id);

  if (action.captured_card_ids.empty()) {
    // No capture: the played card joins the table.
    state.table.push_back(action.played_card_id);
  } else {
    // Capture: take the played card plus every captured table card.
    for (int captured_card_id : action.captured_card_ids) {
      erase_value(state.table, captured_card_id);
      player.captured.push_back(captured_card_id);
    }
    player.captured.push_back(action.played_card_id);
    state.last_capturer = state.current_player;

    // Scopa bonus: clearing the table with a capture scores a bonus, unless
    // this is the very last play of the whole round (no chance for the
    // opponent to respond).
    const bool both_hands_empty = state.players[0].hand.empty() &&
                                  state.players[1].hand.empty();
    const bool is_final_play = both_hands_empty && state.stock.empty();
    if (state.table.empty() && !is_final_play) {
      player.scope += 1;
    }
  }

  state.switch_turn();

  // Mid-round redeal: once both first hands are spent, the 18-card stock is
  // dealt out as a second hand of 9 each.
  if (state.players[0].hand.empty() && state.players[1].hand.empty() &&
      !state.stock.empty()) {
    deal_second_hand(state);
  }

  // End of round: hands and stock both empty. The last player to capture
  // sweeps any cards still on the table (without scoring a scopa for it).
  if (state.players[0].hand.empty() && state.players[1].hand.empty() &&
      state.stock.empty()) {
    if (state.last_capturer >= 0) {
      Player& sweeper = state.players[state.last_capturer];
      for (int card_id : state.table) sweeper.captured.push_back(card_id);
      state.table.clear();
    }
    state.game_over = true;
  }
}

// Return true iff card_id is the 7 of Denari (the Settebello).
static bool is_settebello(const Game_State& state, int card_id) {
  const Card& card = state.all_cards[card_id];
  return card.rank == 7 && card.suit == Suit::DENARI;
}

int compute_primiera(const Game_State& state, int player_index) {
  // Best card per suit: track the maximum primiera value seen for each suit.
  int best_per_suit[4] = {0, 0, 0, 0};
  for (int card_id : state.players[player_index].captured) {
    const Card& card  = state.all_cards[card_id];
    int         value = primiera_value(card.rank);
    int         suit  = (int)card.suit;
    if (value > best_per_suit[suit]) best_per_suit[suit] = value;
  }
  return best_per_suit[0] + best_per_suit[1] + best_per_suit[2] +
         best_per_suit[3];
}

int compute_player_score(const Game_State& state, int player_index) {
  const Player& me  = state.players[player_index];
  const Player& opp = state.players[1 - player_index];

  int my_cards = (int)me.captured.size();
  int op_cards = (int)opp.captured.size();

  int my_denari = 0;
  int op_denari = 0;
  for (int card_id : me.captured)
    if (state.all_cards[card_id].suit == Suit::DENARI) ++my_denari;
  for (int card_id : opp.captured)
    if (state.all_cards[card_id].suit == Suit::DENARI) ++op_denari;

  int score = 0;
  if (my_cards > op_cards) score += 1;
  if (my_denari > op_denari) score += 1;

  for (int card_id : me.captured) {
    if (is_settebello(state, card_id)) {
      score += 1;
      break;
    }
  }

  int my_primiera = compute_primiera(state, player_index);
  int op_primiera = compute_primiera(state, 1 - player_index);
  if (my_primiera > op_primiera) score += 1;

  score += me.scope;
  return score;
}

Choice Game_State::next_choice() {
  // The loop checks is_game_over() first, so this choice is never acted on.
  if (game_over) return Choice{};

  Choice choice;
  choice.player_index     = current_player;
  choice.description      = "play";
  choice.text_description = "Play a card";

  // Each action option is identified by its index in the flat enumeration.
  // We expose one option per action; the label content is unused — agents
  // always pick by index — so a single static tag stands in for every option.
  choice.actions = [](Game& game) -> Choose {
    Game_State&         state   = static_cast<Game_State&>(game);
    std::vector<Action> actions = enumerate_actions(state);
    Choose_Option       option;
    for (int i = 0; i < (int)actions.size(); ++i) {
      option.targets.push_back("action");
    }
    return option;
  };

  choice.resolve = [](Game& game, int index) -> Choice {
    Game_State&         state   = static_cast<Game_State&>(game);
    std::vector<Action> actions = enumerate_actions(state);
    apply_action(state, actions[index]);
    return null_choice;
  };

  return choice;
}

Game_State quick_setup(std::optional<int> seed) {
  Game_State game;
  game.init(seed ? *seed : (int)std::random_device{}());
  return game;
}

void Game_State::init(int seed) {
  std::mt19937 rng((unsigned)seed);

  Game_State game;

  // Id encoding: id / 10 = suit index (0..3), id % 10 = rank - 1.
  const Suit suits[4] = {Suit::COPPE, Suit::DENARI, Suit::SPADE, Suit::BASTONI};
  game.all_cards.reserve(40);
  for (int i = 0; i < 40; ++i) {
    Card card;
    card.id   = i;
    card.rank = (i % 10) + 1;
    card.suit = suits[i / 10];
    game.all_cards.push_back(card);
  }

  std::vector<int> deck(40);
  for (int i = 0; i < 40; ++i) deck[i] = i;
  std::shuffle(deck.begin(), deck.end(), rng);

  Player p0;
  Player p1;
  p0.name = "Player 1";
  p1.name = "Player 2";
  p0.hand.assign(deck.begin(), deck.begin() + 9);
  p1.hand.assign(deck.begin() + 9, deck.begin() + 18);
  game.players = {p0, p1};
  game.table.assign(deck.begin() + 18, deck.begin() + 22);
  game.stock.assign(deck.begin() + 22, deck.end());
  game.current_player = 0;
  game.last_capturer  = -1;

  sort_hand(game, 0);
  sort_hand(game, 1);

  *this = game;
  begin_game();  // The opening decision to present.
}

}  // namespace scopa
