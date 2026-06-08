#include "gameplay.h"

#include <algorithm>
#include <random>

namespace tressette {

std::vector<Card> all_cards;

int trick_winner(const Game_State& state) {
  // Caller guarantees state.trick.size() == 2.
  const int   led_card_id      = state.trick[0];
  const int   response_card_id = state.trick[1];
  const Card& led_card         = all_cards[led_card_id];
  const Card& response_card    = all_cards[response_card_id];
  const int   leader           = state.trick_leader;
  // Different suit -> leader wins automatically (no trumps in Tressette).
  if (response_card.suit != led_card.suit) return leader;
  // Same suit -> stronger rank wins.
  return strength(response_card.rank) > strength(led_card.rank) ? 1 - leader
                                                                : leader;
}

int compute_player_score(const Game_State& state, int player_index) {
  int thirds = 0;
  for (int cid : state.players[player_index].tricks_won) {
    thirds += card_thirds(all_cards[cid].rank);
  }
  int score = thirds / 3;  // floor.
  if (state.stock.empty() && state.players[player_index].hand.empty() &&
      state.last_trick_winner == player_index)
    score += 1;  // ultima bonus.
  return score;
}

std::vector<int> legal_cards(const Game_State& state) {
  const Player& player = state.players[state.current_player];
  // Leader can play any card.
  if (state.trick.empty()) {
    return std::vector<int>(player.hand.begin(), player.hand.end());
  }

  const Suit led_suit = all_cards[state.trick[0]].suit;
  auto       matches  = std::vector<int>();
  for (int cid : player.hand) {
    if (all_cards[cid].suit == led_suit) matches.push_back(cid);
  }
  if (!matches.empty()) return matches;
  return std::vector<int>(player.hand.begin(), player.hand.end());
}

// Remove a single occurrence of card_id from a hand.
static void erase_card(Inlined_Vector<int, 10>& hand, int card_id) {
  auto it = std::find(hand.begin(), hand.end(), card_id);
  if (it != hand.end()) hand.erase(it);
}

void sort_hand(Game_State& state, int player_index) {
  auto& hand = state.players[player_index].hand;
  std::sort(hand.begin(), hand.end(), [](int a, int b) {
    const Card& ca = all_cards[a];
    const Card& cb = all_cards[b];
    if (ca.suit != cb.suit) return ca.suit < cb.suit;
    return ca.rank < cb.rank;
  });
}

static Choice wait_for_player_acknolwdgment(Game_State& state);

Choice make_play_choice(Game_State& state);

Choice play_card(Game_State& state, int card_id) {
  Player& player = state.players[state.current_player];
  erase_card(player.hand, card_id);
  state.trick.push_back(card_id);

  if (state.trick.size() < 2) {
    // First card of the trick: the responder plays next.
    state.switch_turn();
      return make_play_choice(state);
  } else {
    return wait_for_player_acknolwdgment(state);
  }
}

static Choice wait_for_player_acknolwdgment(Game_State& state) {
  auto choice = Choice();
  // The human confirms the completed trick when there is one; in headless play
  // (search, self-play) there is no human, so let the current seat own this
  // single-option step rather than the invalid seat -1.
  choice.player_index =
    state.human_player >= 0 ? state.human_player : state.current_player;
  choice.description      = "acknowledge";
  choice.text_description = "Acknowledge trick";

  choice.actions = [](Game& g) -> Choose {
    auto& s   = static_cast<Game_State&>(g);
    auto  c   = Choose_Option();
    c.targets = {"Ok"};
    return c;
  };

  choice.resolve = [](Game& g, int _index) -> Choice {
    auto& state = static_cast<Game_State&>(g);

    // Trick complete: resolve it.
    const int winner        = trick_winner(state);
    state.last_trick_winner = winner;
    Player& winner_p        = state.players[winner];
    for (int cid : state.trick) winner_p.tricks_won.push_back(cid);

    state.trick.clear();
    state.trick_leader   = winner;
    state.current_player = winner;

    // Draw phase: winner draws first, loser second.
    if (!state.stock.empty()) {
      int top = state.stock.back();
      state.stock.pop_back();
      state.players[winner].hand.push_back(top);

      top = state.stock.back();
      state.stock.pop_back();
      state.players[1 - winner].hand.push_back(top);

      sort_hand(state, winner);
      sort_hand(state, 1 - winner);
    }

    // Game ends when both hands are empty.
    if (state.players[0].hand.empty() && state.players[1].hand.empty()) {
      state.game_over = true;
    }
    return make_play_choice(state);
  };
  return choice;
}

void resolve_pending_trick(Game_State& state) {
  // Stub: pending_trick_resolve is not currently used by the game loop.
  (void)state;
}

Choice Game_State::next_choice() {
  // A complete trick (2 cards on the table) must be resolved before anyone
  // plays again: that's where the winner is decided, the cards are won, and
  // both players draw from the stock. The search agents reach the pending
  // choice only through next_choice(), so without this the trick would never
  // resolve during search and every position would score zero.
  if (trick.size() == 2) return wait_for_player_acknolwdgment(*this);
  return make_play_choice(*this);
}

Choice make_play_choice(Game_State& state) {
  auto choice             = Choice();
  choice.player_index     = state.current_player;
  choice.description      = "play";
  choice.text_description = "Play a card";

  choice.actions = [](Game& g) -> Choose {
    auto& s   = static_cast<Game_State&>(g);
    auto  c   = Choose_Card();
    c.targets = legal_cards(s);
    return c;
  };

  choice.resolve = [](Game& g, int index) -> Choice {
    auto&            s           = static_cast<Game_State&>(g);
    std::vector<int> legal       = legal_cards(s);
    auto             next_choice = play_card(s, legal[index]);
    return next_choice;
  };

  return choice;
}

Game_State quick_setup(std::optional<int> seed) {
  auto rng = std::mt19937(seed ? (unsigned)*seed : std::random_device{}());

  auto game = Game_State();

  // Card id encoding: id / 10 = suit index (0..3), id % 10 = rank - 1.
  const Suit suits[4] = {Suit::COPPE, Suit::DENARI, Suit::SPADE, Suit::BASTONI};
  all_cards.clear();
  all_cards.reserve(40);
  for (int i = 0; i < 40; ++i) {
    auto c = Card();
    c.id   = i;
    c.rank = (i % 10) + 1;
    c.suit = suits[i / 10];
    all_cards.push_back(c);
  }

  auto deck = std::vector<int>(40);
  for (int i = 0; i < 40; ++i) deck[i] = i;
  std::shuffle(deck.begin(), deck.end(), rng);

  auto p0 = Player();
  auto p1 = Player();
  p0.name = "Player 1";
  p1.name = "Player 2";
  p0.hand.assign(deck.begin(), deck.begin() + 10);
  p1.hand.assign(deck.begin() + 10, deck.begin() + 20);
  game.players = {p0, p1};
  game.stock.assign(deck.begin() + 20, deck.end());
  game.current_player = 0;
  game.trick_leader   = 0;

  sort_hand(game, 0);
  sort_hand(game, 1);
  game.begin_game(game.next_choice());  // The opening decision to present.
  return game;
}

}  // namespace tressette
