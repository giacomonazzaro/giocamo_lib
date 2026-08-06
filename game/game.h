#pragma once

#include <basic/array_inline.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Card targets live inline (capacity 16) so producing the action list for a
// choice does no heap allocation — this happens on every simulated ply during
// search. Spills to the heap on the rare hand with more than 16 options.
struct Choose_Card {
  Array_Inline<int, 16> targets;
  bool                  up_to = true;
};

struct Choose_Cards {
  Array_Inline<int, 16> targets;
  int                   count;
  bool                  up_to = true;
};

// Option labels are string literals (static lifetime), held as non-owning
// pointers so building a choice allocates nothing — same reasoning as the int
// target lists above.
struct Choose_Option {
  Array_Inline<const char*, 16> targets;
};

struct Choose_Options {
  Array_Inline<const char*, 16> targets;
  int                           count;
  bool                          up_to = true;
};

using Choose =
  std::variant<Choose_Card, Choose_Cards, Choose_Option, Choose_Options>;

struct Game;
struct Choice;

struct Choice {
  int player_index;
  // Set from string literals (static lifetime), so these are non-owning views —
  // a Choice is built on every simulated ply during search, and a std::string
  // member would allocate each time. string_view compares by content, so `==`
  // against a literal works as expected.
  std::string_view description;
  std::string_view text_description;
  // actions: produces the set of available action options for this choice.
  std::function<Choose(Game&)> actions;
  // resolve: applies the chosen action and returns any follow-up choices.
  std::function<Choice(Game&, int)> resolve;
};

// Abstract base. Concrete games (e.g. gods) subclass and override.
//
// A game holds exactly one pending choice at a time. Resolving it produces the
// next one, so the whole game is the sequence
//   begin_game -> resolve -> resolve -> ... -> is_game_over.
// Everything that walks a game — the app loop, minimax, mcts, self-play — reads
// the pending choice with pending_choice() and advances with resolve_choice(),
// so they all see the same sequence of decisions. A game may have its own
// next_choice() to build a choice during setup, but nothing outside the game
// calls it: doing so would advance a game whose next_choice() has side effects
// (draining a queue of card effects, resolving a trick) past a decision the app
// loop would have presented.
struct Game {
  Choice _choice;

  virtual ~Game()                   = default;
  virtual bool is_game_over() const = 0;

  // Seeds the first choice to present, before the game loop starts. Every later
  // choice comes from a resolve, so this is only needed once during setup.
  void begin_game(const Choice& choice) { _choice = choice; }
};

// The choice the game is currently waiting on.
inline const Choice& pending_choice(const Game& game) { return game._choice; }

// Returns the number of indexable action options for a Choose.
// For multi-select kinds this is the count of valid combinations.
int action_options_count(const Choose& choose);

// Number of options the pending choice offers. Zero when the game has no choice
// to present, so callers can test this instead of guarding against an empty
// choice themselves.
int pending_action_count(Game& game);

// Applies the pending choice's resolve callback; the choice it returns becomes
// the new pending choice.
void resolve_choice(Game& game, int index);

// Forward declaration; defined in agent.h.
struct Agent;

void game_loop(
  Game& game, Agent& agent, std::function<void(Game&)> callback = nullptr
);
bool game_frame(Game& game, Agent& agent);
