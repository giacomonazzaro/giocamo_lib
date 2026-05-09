#pragma once

#include <nanobind/nanobind.h>

#include <memory>
#include <string>
#include <vector>

#include "models.h"

namespace nb = nanobind;

// Replace the global card_designs registry with the provided list.
// Each entry is a (name, type_str, color_str, effect) tuple; ids are assigned
// in order, starting from 0.
void set_card_designs(
  const std::vector<std::tuple<std::string, std::string, std::string, std::string>>& entries
);

void bind_setup(nb::module_& m);
