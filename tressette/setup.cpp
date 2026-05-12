#include "setup.h"

#include <algorithm>
#include <random>

namespace tressette {

Game_State quick_setup(std::optional<int> seed) {
  std::mt19937 rng(seed ? (unsigned)*seed : std::random_device{}());

  Game_State game;

  // Card id encoding: id / 10 = suit index (0..3), id % 10 = rank - 1.
  const Suit suits[4] = {Suit::COPPE, Suit::DENARI, Suit::SPADE, Suit::BASTONI};
  game.all_cards.reserve(40);
  for (int i = 0; i < 40; ++i) {
    Card c;
    c.id   = i;
    c.rank = (i % 10) + 1;
    c.suit = suits[i / 10];
    game.all_cards.push_back(c);
  }

  std::vector<int> deck(40);
  for (int i = 0; i < 40; ++i) deck[i] = i;
  std::shuffle(deck.begin(), deck.end(), rng);

  Player p0, p1;
  p0.name = "Player 1";
  p1.name = "Player 2";
  p0.hand.assign(deck.begin(),      deck.begin() + 10);
  p1.hand.assign(deck.begin() + 10, deck.begin() + 20);
  game.players = {p0, p1};
  game.stock.assign(deck.begin() + 20, deck.end());
  game.current_player = 0;
  game.trick_leader   = 0;
  return game;
}

}  // namespace tressette
