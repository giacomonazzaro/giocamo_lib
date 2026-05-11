#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "models.h"

// Factory: build the right Card_Design subclass for a given card name.
// Falls back to the base Card_Design if name is not in the registry.
std::unique_ptr<Card_Design> create_card_design(
  const std::string& name,
  const std::string& type_str,
  const std::string& color_str,
  const std::string& effect,
  int                id
);

// Returns true if a specialized class exists for this card name.
bool has_card_class(const std::string& name);
