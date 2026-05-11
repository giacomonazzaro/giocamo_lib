#include "setup.h"

#include "cards.h"
#include "models.h"

void set_card_designs(
  const std::vector<
    std::tuple<std::string, std::string, std::string, std::string>>& entries
) {
  card_designs.clear();
  card_designs.reserve(entries.size());
  int id = 0;
  for (const auto& e : entries) {
    card_designs.push_back(create_card_design(
      std::get<0>(e), std::get<1>(e), std::get<2>(e), std::get<3>(e), id
    ));
    ++id;
  }
}
