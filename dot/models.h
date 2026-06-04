#pragma once

namespace dot {

// A D.O.T card. Every card shows some blue, black and red dots.
// Regular draw cards have a random 0..3 of each color. A star card has
// exactly 4 dots of a single color (the other two colors are 0).
struct Card {
  int  id         = 0;  // 0..17, index into the deck vector.
  int  blue_dots  = 0;  // 0..3, or 4 on the blue star card.
  int  black_dots = 0;  // 0..3, or 4 on the black star card.
  int  red_dots   = 0;  // 0..3, or 4 on the red star card.
  bool is_star    = false;
};

}  // namespace dot
