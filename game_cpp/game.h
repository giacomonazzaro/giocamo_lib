#pragma once

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct Choose_Card {
  std::vector<int> targets;
  bool             up_to = true;
};

struct Choose_Cards {
  std::vector<int> targets;
  int              count;
  bool             up_to = true;
};

struct Choose_Option {
  std::vector<std::string> targets;
};

struct Choose_Options {
  std::vector<std::string> targets;
  int                      count;
  bool                     up_to = true;
};

using Choose = std::variant<Choose_Card, Choose_Cards, Choose_Option, Choose_Options>;

struct Game;
struct Choice;

struct Choice {
  int         player_index;
  std::string description;
  std::string text_description;
  // actions: produces the set of available action options for this choice.
  std::function<Choose(Game&)> actions;
  // resolve: applies the chosen action and returns any follow-up choices.
  std::function<std::vector<Choice>(Game&, int)> resolve;
};

// Abstract base. Concrete games (e.g. gods) subclass and override.
struct Game {
  std::vector<Choice> choices;

  virtual ~Game() = default;

  virtual bool                  is_game_over() const = 0;
  virtual std::optional<Choice> next_choice()        = 0;
};

// Returns the number of indexable action options for a Choose.
// For multi-select kinds this is the count of valid combinations.
int action_options_count(const Choose& choose);

// Applies a choice's resolve callback and appends any follow-up choices.
void resolve_choice(Game& game, const Choice& choice, int index);

// Forward declaration; defined in agent.h.
struct Agent;

void                  game_loop(Game& game, Agent& agent, std::function<void(Game&)> callback = nullptr);
std::optional<Choice> game_frame(Game& game, Agent& agent, std::optional<Choice> choice);
