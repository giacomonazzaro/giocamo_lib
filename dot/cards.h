#pragma once

#include <dot/models.h>

#include <vector>

namespace dot {

// Build one deck of 18 cards from a seed: 15 draw cards with each color's
// dots drawn uniformly in 0..3, followed by 3 star cards (4 blue, 4 black,
// 4 red). Both players use an identical deck, so call this with the same
// seed for each.
std::vector<Card> make_deck(int seed);

}  // namespace dot
