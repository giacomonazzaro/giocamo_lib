#pragma once

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "inlined_vector.h"

// Card targets live inline (capacity 16) so producing the action list for a
// choice does no heap allocation — this happens on every simulated ply during
// search. Spills to the heap on the rare hand with more than 16 options.
struct Choose_Card {
  Inlined_Vector<int, 16> targets;
  bool                    up_to = true;
};

struct Choose_Cards {
  Inlined_Vector<int, 16> targets;
  int                     count;
  bool                    up_to = true;
};

// Option labels are string literals (static lifetime), held as non-owning
// pointers so building a choice allocates nothing — same reasoning as the int
// target lists above.
struct Choose_Option {
  Inlined_Vector<const char*, 16> targets;
};

struct Choose_Options {
  Inlined_Vector<const char*, 16> targets;
  int                             count;
  bool                            up_to = true;
};

using Choose =
  std::variant<Choose_Card, Choose_Cards, Choose_Option, Choose_Options>;

struct Game;
struct Choice;

struct Choice {
  int         player_index;
  const char* description      = "";
  const char* text_description = "";
  // actions: produces the set of available action options for this choice.
  std::function<Choose(Game&)> actions;
  // resolve: applies the chosen action and returns any follow-up choices.
  std::function<Choice(Game&, int)> resolve;
};

// Abstract base. Concrete games (e.g. gods) subclass and override.
struct Game {
  Choice _choice;

  virtual ~Game()                   = default;
  virtual bool is_game_over() const = 0;

  // Seeds the first choice to present, before the game loop starts. Every later
  // choice comes from a resolve, so this is only needed once during setup.
  void begin_game(const Choice& choice) { _choice = choice; }

  inline const Choice& current_choice() const { return _choice; }
};

// Returns the number of indexable action options for a Choose.
// For multi-select kinds this is the count of valid combinations.
int action_options_count(const Choose& choose);

// Applies a choice's resolve callback and appends any follow-up choices.
void resolve_choice(Game& game, const Choice& choice, int index);

// Forward declaration; defined in agent.h.
struct Agent;

void game_loop(
  Game& game, Agent& agent, std::function<void(Game&)> callback = nullptr
);
bool game_frame(Game& game, Agent& agent);
