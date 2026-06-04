#include "cards.h"

#include <random>

namespace dot {

std::vector<Card> make_deck(int seed) {
  auto deck = std::vector<Card>();
  auto rng  = std::mt19937(seed);
  auto dots = std::uniform_int_distribution<int>(0, 3);

  // 15 draw cards with random dots.
  for (int i = 0; i < 15; i++) {
    Card card;
    card.id         = (int)deck.size();
    card.blue_dots  = dots(rng);
    card.black_dots = dots(rng);
    card.red_dots   = dots(rng);
    deck.push_back(card);
  }

  // 3 star cards: one per color, each with 4 dots of that color.
  Card blue_star;
  blue_star.id        = (int)deck.size();
  blue_star.blue_dots = 4;
  blue_star.is_star   = true;
  deck.push_back(blue_star);

  Card black_star;
  black_star.id         = (int)deck.size();
  black_star.black_dots = 4;
  black_star.is_star    = true;
  deck.push_back(black_star);

  Card red_star;
  red_star.id       = (int)deck.size();
  red_star.red_dots = 4;
  red_star.is_star  = true;
  deck.push_back(red_star);

  return deck;
}

}  // namespace dot
