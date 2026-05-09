#include "setup.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

#include "cards.h"
#include "models.h"

namespace nb = nanobind;
using namespace nb::literals;

void set_card_designs(
  const std::vector<std::tuple<std::string, std::string, std::string, std::string>>& entries
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

void bind_setup(nb::module_& m) {
  m.def("set_card_designs", &set_card_designs, "entries"_a,
        "Replace the global card_designs registry. Each entry is "
        "(name, type, color, effect); ids assigned in order from 0.");
}
