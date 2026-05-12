#pragma once

#include <optional>

#include "models.h"

namespace tressette {

// Deal a fresh hand: 10 cards each, 20 in the stock, player 0 leads.
Game_State quick_setup(std::optional<int> seed = std::nullopt);

}  // namespace tressette
