#include "models.h"

Table_State::Table_State()
    : is_drop_card_allowed([](int, int, int) { return true; }) {}

std::optional<std::tuple<int, int, int>> Table_State::poll_dropped_card() {
  auto result  = dropped_card;
  dropped_card = std::nullopt;
  return result;
}
