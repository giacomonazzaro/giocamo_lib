#pragma once

#include <algorithm>
#include <optional>

#include <game/game.h>

#include "models.h"

namespace tressette {

// Determine the winner of the current 2-card trick.
// state.trick must contain exactly 2 card ids; the first one is the led card
// played by state.trick_leader, the second by 1 - state.trick_leader.
// Returns the winning player index (0 or 1).
inline int trick_winner(const Game_State& state) {
  // Caller guarantees state.trick.size() == 2.
  const int   led_card_id      = state.trick[0];
  const int   response_card_id = state.trick[1];
  const Card& led_card         = state.all_cards[led_card_id];
  const Card& response_card    = state.all_cards[response_card_id];
  const int   leader           = state.trick_leader;
  // Different suit -> leader wins automatically (no trumps in Tressette).
  if (response_card.suit != led_card.suit) return leader;
  // Same suit -> stronger rank wins.
  return strength(response_card.rank) > strength(led_card.rank) ? 1 - leader
                                                                : leader;
}

// Total points scored by player_index, integer (floored thirds + ultima bonus).
inline int compute_player_score(const Game_State& state, int player_index) {
  int thirds = 0;
  for (int cid : state.players[player_index].tricks_won) {
    thirds += card_thirds(state.all_cards[cid].rank);
  }
  int score = thirds / 3;                                   // floor.
  if (state.last_trick_winner == player_index) score += 1;  // ultima bonus.
  return score;
}

// Returns the legal cards in the current player's hand for the next play.
// During the draw phase (stock non-empty) any card is legal. After the stock
// is empty, the responder must follow suit if possible.
inline static std::vector<int> legal_cards(const Game_State& state) {
  const Player& player        = state.players[state.current_player];
  const bool must_follow_suit = state.stock.empty() && state.trick.size() == 1;
  if (!must_follow_suit) return player.hand;

  const Suit       led_suit = state.all_cards[state.trick[0]].suit;
  std::vector<int> matches;
  for (int cid : player.hand) {
    if (state.all_cards[cid].suit == led_suit) matches.push_back(cid);
  }
  if (!matches.empty()) return matches;
  return player.hand;
}

// Helper: remove card_id from a vector<int> (single occurrence).
inline static void erase_card(std::vector<int>& v, int card_id) {
  auto it = std::find(v.begin(), v.end(), card_id);
  if (it != v.end()) v.erase(it);
}

// Apply the play of card_id by the current player.
//   - Removes card_id from current player's hand and pushes onto trick.
//   - If trick is now complete, resolves it: assigns winner, draws from stock
//     (winner first, loser second), checks for game-over.
//   - Otherwise switches to the other player.
//   - Fires on_cards_changed once at the end.
inline void play_card(Game_State& state, int card_id) {
  Player& player = state.players[state.current_player];
  erase_card(player.hand, card_id);
  state.trick.push_back(card_id);

  if (state.trick.size() < 2) {
    // First card of the trick: the responder plays next.
    state.switch_turn();
    state.notify_cards_changed();
    return;
  }

  // Trick complete: resolve it.
  const int winner        = trick_winner(state);
  state.last_trick_winner = winner;
  Player& winner_p        = state.players[winner];
  for (int cid : state.trick) winner_p.tricks_won.push_back(cid);
  state.trick.clear();
  state.trick_leader   = winner;
  state.current_player = winner;

  // Draw phase: winner draws first, loser second (if cards remain).
  if (!state.stock.empty()) {
    int top = state.stock.back();
    state.stock.pop_back();
    state.players[winner].hand.push_back(top);
  }
  if (!state.stock.empty()) {
    int top = state.stock.back();
    state.stock.pop_back();
    state.players[1 - winner].hand.push_back(top);
  }

  // Game ends when both hands are empty.
  if (state.players[0].hand.empty() && state.players[1].hand.empty()) {
    state.game_over = true;
  }
  state.notify_cards_changed();
}

// Finalize a trick that's been held on the table after both cards were played.
// No-op if pending_trick_resolve is false.
inline void resolve_pending_trick(Game_State& state) {
  // Stub: pending_trick_resolve is not currently used by the game loop.
  (void)state;
}

inline std::optional<Choice> Game_State::next_choice() {
  if (game_over) return std::nullopt;

  Choice choice;
  choice.player_index     = current_player;
  choice.description      = "play";
  choice.text_description = "Play a card";

  choice.actions = [](Game& g) -> Choose {
    auto&       s = static_cast<Game_State&>(g);
    Choose_Card c;
    c.targets = legal_cards(s);
    return c;
  };

  choice.resolve = [](Game& g, int index) -> std::vector<Choice> {
    auto&            s     = static_cast<Game_State&>(g);
    std::vector<int> legal = legal_cards(s);
    play_card(s, legal[index]);
    return {};
  };

  return choice;
}

}  // namespace tressette
