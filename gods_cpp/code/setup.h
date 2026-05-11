#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "models.h"

// Replace the global card_designs registry with the provided list.
// Each entry is a (name, type_str, color_str, effect) tuple; ids are assigned
// in order, starting from 0.
void set_card_designs(
  const std::vector<
    std::tuple<std::string, std::string, std::string, std::string>>& entries
);
