#include "gameplay.h"

#include <dot/cards.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace dot {

constexpr int DRAW_PER_ROUND = 5;  // Draw cards drawn each round (star adds 1).
constexpr int NUM_ROUNDS      = 3;
constexpr int WIN_TOKENS      = 5;  // Reaching this ends the game immediately.

// Binomial coefficient C(n, k). Matches the counting in game/game.cpp.
static long long binomial(int n, int k) {
  if (k < 0 || k > n) return 0;
  if (k == 0 || k == n) return 1;
  if (k > n - k) k = n - k;
  long long result = 1;
  for (int i = 0; i < k; ++i) result = result * (n - i) / (i + 1);
  return result;
}

std::vector<int> combination_at(int n, int k, long long index) {
  // k >= n: the only combination is everything.
  if (k >= n) {
    std::vector<int> all;
    for (int i = 0; i < n; ++i) all.push_back(i);
    return all;
  }
  std::vector<int> result;
  int              start = 0;
  for (int chosen = 0; chosen < k; ++chosen) {
    for (int value = start; value < n; ++value) {
      long long count = binomial(n - 1 - value, k - 1 - chosen);
      if (index < count) {
        result.push_back(value);
        start = value + 1;
        break;
      }
      index -= count;
    }
  }
  return result;
}

long long combination_rank(int n, const std::vector<int>& positions) {
  int       k     = (int)positions.size();
  long long index = 0;
  int       start = 0;
  for (int chosen = 0; chosen < k; ++chosen) {
    for (int value = start; value < positions[chosen]; ++value) {
      index += binomial(n - 1 - value, k - 1 - chosen);
    }
    start = positions[chosen] + 1;
  }
  return index;
}

// Dots of one color in a set of cards. color: 0 blue, 1 black, 2 red.
static int color_count(
  const Game_State& state, const std::vector<int>& cards, int color
) {
  int total = 0;
  for (int id : cards) {
    const Card& card = state.all_cards[id];
    if (color == 0) total += card.blue_dots;
    else if (color == 1) total += card.black_dots;
    else total += card.red_dots;
  }
  return total;
}

int total_tokens(const Game_State& state, int player) {
  const Player& p = state.players[player];
  return p.tokens_blue + p.tokens_black + p.tokens_red;
}

int compute_player_score(const Game_State& state, int player) {
  return total_tokens(state, player);
}

int discard_count(const Game_State& state) {
  return state.round + 1;  // 1 after Round 1, 2 after Round 2.
}

// Reference to a player's token count for one color, so we can add to it.
static int& tokens_for_color(Player& player, int color) {
  if (color == 0) return player.tokens_blue;
  if (color == 1) return player.tokens_black;
  return player.tokens_red;
}

// Award one color's pending tokens. Rounds 1 & 3 reward the largest dot-count
// difference vs the shared pool; Round 2 rewards the smallest. A tie leaves
// the tokens pending so they carry into the next round.
static void award_color(Game_State& state, int color, int& pending) {
  int shared = color_count(state, state.shared_pool, color);
  int diff_0 = std::abs(color_count(state, state.players[0].pool, color) - shared);
  int diff_1 = std::abs(color_count(state, state.players[1].pool, color) - shared);
  if (diff_0 == diff_1) return;  // Tie: tokens carry over to the next round.

  bool reward_smallest = (state.round == 1);  // Round 2 wants the smallest.
  int  winner          = reward_smallest ? (diff_0 < diff_1 ? 0 : 1)
                                         : (diff_0 > diff_1 ? 0 : 1);
  tokens_for_color(state.players[winner], color) += pending;
  pending = 0;
}

static void do_scoring(Game_State& state) {
  award_color(state, 0, state.pending_blue);
  award_color(state, 1, state.pending_black);
  award_color(state, 2, state.pending_red);

  // The game ends after Round 3, or as soon as a player reaches 5 tokens.
  if (state.round == NUM_ROUNDS - 1 || total_tokens(state, 0) >= WIN_TOKENS ||
      total_tokens(state, 1) >= WIN_TOKENS) {
    state.game_over = true;
  }
}

// Who discards first this round: the player with the most tokens, breaking
// ties by most blue, then most black, then defaulting to player 0.
static int compute_discard_first(const Game_State& state) {
  int total_0 = total_tokens(state, 0);
  int total_1 = total_tokens(state, 1);
  if (total_0 != total_1) return total_0 > total_1 ? 0 : 1;
  if (state.players[0].tokens_blue != state.players[1].tokens_blue)
    return state.players[0].tokens_blue > state.players[1].tokens_blue ? 0 : 1;
  if (state.players[0].tokens_black != state.players[1].tokens_black)
    return state.players[0].tokens_black > state.players[1].tokens_black ? 0 : 1;
  return 0;
}

// Begin a round: clear the shared pool, add a fresh token of each color, and
// deal each player a new hand of 5 draw cards plus 1 star card.
static void start_round(Game_State& state) {
  state.shared_pool.clear();
  state.pending_blue += 1;
  state.pending_black += 1;
  state.pending_red += 1;
  for (Player& player : state.players) {
    // Everything carried into this round is already known and shown.
    player.revealed_pool_count = (int)player.pool.size();
    player.hand.clear();
    for (int i = 0; i < DRAW_PER_ROUND && !player.draw_deck.empty(); ++i) {
      player.hand.push_back(player.draw_deck.back());
      player.draw_deck.pop_back();
    }
    if (!player.star_deck.empty()) {
      player.hand.push_back(player.star_deck.back());
      player.star_deck.pop_back();
    }
  }
  state.phase         = Phase::SPLIT;
  state.acting_player = 0;
}

Game_State quick_setup(int seed) {
  Game_State state;
  state.players.resize(2);

  // Both players get an identical deck of cards, with distinct ids so each
  // card is its own Thing on the table.
  std::vector<Card> base = make_deck(seed);
  for (int player_index = 0; player_index < 2; ++player_index) {
    Player& player = state.players[player_index];
    for (const Card& card : base) {
      Card copy = card;
      copy.id   = (int)state.all_cards.size();
      state.all_cards.push_back(copy);
      if (copy.is_star) player.star_deck.push_back(copy.id);
      else player.draw_deck.push_back(copy.id);
    }
    // Shuffle the draw pile with a per-player stream so the two orders differ.
    auto rng = std::mt19937((unsigned)seed + 1 + player_index);
    std::shuffle(player.draw_deck.begin(), player.draw_deck.end(), rng);
  }

  state.round = 0;
  start_round(state);
  return state;
}

// Move the acting player's chosen 3 cards to the shared pool and the other 3
// to their own pool, then advance: the other player splits, or once both have,
// score the round and set up the discard phase (or end the game).
static void resolve_split(Game_State& state, int index) {
  Player&          player = state.players[state.acting_player];
  std::vector<int> picks  = combination_at((int)player.hand.size(), SHARED_COUNT, index);
  std::vector<bool> to_shared(player.hand.size(), false);
  for (int position : picks) to_shared[position] = true;
  for (size_t i = 0; i < player.hand.size(); ++i) {
    if (to_shared[i]) state.shared_pool.push_back(player.hand[i]);
    else player.pool.push_back(player.hand[i]);
  }
  player.hand.clear();

  if (state.acting_player == 0) {
    state.acting_player = 1;  // The other player splits next.
    return;
  }
  // Both players have committed; pause on the revealed shared pool so the
  // player can see what the opponent played before it is scored.
  state.phase = Phase::ACKNOWLEDGE;
  state.acting_player =
    state.human_player >= 0 ? state.human_player : state.acting_player;
}

// After the player has seen the revealed board: score the round and set up the
// discard phase, or end the game.
static void resolve_acknowledge(Game_State& state) {
  do_scoring(state);
  if (state.game_over) return;
  state.phase         = Phase::DISCARD;
  state.discard_first = compute_discard_first(state);
  state.acting_player = state.discard_first;
}

// Remove the chosen cards from the opponent's pool, then advance: the other
// player discards, or once both have, start the next round.
static void resolve_discard(Game_State& state, int index) {
  std::vector<int>& opponent_pool = state.players[1 - state.acting_player].pool;
  std::vector<int>  picks =
    combination_at((int)opponent_pool.size(), discard_count(state), index);
  // Erase from the back so earlier positions stay valid.
  std::sort(picks.rbegin(), picks.rend());
  for (int position : picks) opponent_pool.erase(opponent_pool.begin() + position);

  if (state.acting_player == state.discard_first) {
    state.acting_player = 1 - state.discard_first;  // The other player discards.
    return;
  }
  state.round += 1;
  start_round(state);
}

Choice Game_State::next_choice() {
  // The game is driven by is_game_over(); when it's set, this choice is never
  // acted on, so a blank one is fine.
  if (game_over) return Choice{};

  Choice choice;
  choice.player_index = acting_player;

  if (phase == Phase::SPLIT) {
    choice.description      = "split";
    choice.text_description = "Pick 3 cards for the shared pool";
    choice.actions          = [](Game& game) -> Choose {
      Game_State& state = static_cast<Game_State&>(game);
      return Choose_Cards{state.players[state.acting_player].hand, SHARED_COUNT, false};
    };
    choice.resolve = [](Game& game, int index) -> Choice {
      Game_State& state = static_cast<Game_State&>(game);
      resolve_split(state, index);
      return state.next_choice();  // The next decision (or game over).
    };
  } else if (phase == Phase::ACKNOWLEDGE) {
    choice.description      = "acknowledge";
    choice.text_description = "See the revealed cards";
    choice.actions          = [](Game&) -> Choose {
      Choose_Option option;
      option.targets = {"Ok"};
      return option;
    };
    choice.resolve = [](Game& game, int) -> Choice {
      Game_State& state = static_cast<Game_State&>(game);
      resolve_acknowledge(state);
      return state.next_choice();  // The next decision (or game over).
    };
  } else {
    choice.description      = "discard";
    choice.text_description = "Discard from the opponent's pool";
    choice.actions          = [](Game& game) -> Choose {
      Game_State& state = static_cast<Game_State&>(game);
      return Choose_Cards{
        state.players[1 - state.acting_player].pool, discard_count(state), false
      };
    };
    choice.resolve = [](Game& game, int index) -> Choice {
      Game_State& state = static_cast<Game_State&>(game);
      resolve_discard(state, index);
      return state.next_choice();  // The next decision (or game over).
    };
  }
  return choice;
}

}  // namespace dot
