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
  int player_index = -1;
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

  bool is_null() const {
    return player_index == -1 && description.empty() && !actions && !resolve;
  }
};
inline bool operator==(const Choice& a, const Choice& c) {
  return a.player_index == c.player_index && a.description == c.description &&
         a.text_description == c.text_description;
}

// Abstract base. A concrete game derives from it.
//
// A game waits on one choice at a time. Resolving the pending choice produces
// the next one. A game therefore runs as:
//   begin_game -> resolve -> resolve -> ... -> is_game_over.
//
// Two functions do all the work:
//   pending_choice(game) reads the choice the game waits on. It changes
//     nothing. Any caller may read it, at any time, as often as it likes.
//   resolve_choice(game, index) applies a choice and moves the game forward.
//     It is the only way to move a game forward.
//
// A resolve often knows which choice comes next, and returns it. A resolve
// that does not know returns null_choice instead. resolve_choice then asks the
// game, by calling next_choice().
//
// next_choice() is allowed to change the game. It may take a pending effect
// off a queue, or settle a step the game had left open. It therefore returns a
// different choice on every call, and it must be called exactly once per
// decision. resolve_choice is the only caller, which is what makes that true
// for every caller at once: the app loop, minimax, mcts and self-play all move
// a game forward through resolve_choice, and so all walk the same choices.
struct Game {
  Choice _choice;

  virtual ~Game()                     = default;
  virtual bool   is_game_over() const = 0;
  virtual Choice next_choice()        = 0;

  // Set the game up — deal the cards, place the pieces — and ask for the
  // opening choice with begin_game(). play_game calls it once, before anything
  // else reads the state. A game with nothing to randomize ignores `seed`.
  virtual void init(int seed = 0) {}

  // Asks the game for its opening choice. Setup calls this once, after it has
  // dealt the cards or set up the board.
  void begin_game() { _choice = next_choice(); }
};

// What a resolve returns when it cannot say which choice comes next, and what
// next_choice() returns once the game is over.
inline const Choice null_choice = Choice{
  .player_index     = -1,
  .description      = "",
  .text_description = "",
  .actions          = nullptr,
  .resolve          = nullptr
};

// ---- What a game must provide ----
//
// minimax.h and mcts.h are templates. Each one takes the concrete game type as
// a parameter. Their requirements are therefore spread across template bodies.
// This is the whole list.
//
// Every game must provide:
//
//   struct My_Game : Game {
//     bool   is_game_over() const override;
//     Choice next_choice() override;
//   };
//
//   next_choice() returns the choice the game waits on next, and returns
//   null_choice once the game is over. It may change the game while it works
//   one out. Only resolve_choice calls it, so it runs once per decision.
//
//   Each resolve returns the choice that follows it. A resolve that cannot work
//   that out returns null_choice, and resolve_choice falls back to
//   next_choice().
//
//   The type must be copyable. A search copies a whole position per child node
//   instead of undoing moves.
//
//   Setup calls begin_game() once, after it has dealt the cards or set up the
//   board, and before any loop or search touches the game.
//
// minimax.h and mcts.h need one free function. Declare it in the same
// namespace as the game type, because that is where a search looks for it:
//
//   float evaluate_state(My_Game& state, int player_index);
//
//   evaluate_state rates `state` for player_index. Higher is better. A search
//   calls it at the bottom of every branch it looks at, so keep it cheap. A won
//   position must rate above every unfinished position, and a lost position
//   must rate below every unfinished position. A const reference also works.
//
// The _Stochastic agents need one more free function. A game with no hidden
// information never uses those agents, and such a game may leave this out:
//
//   My_Game sample_state(
//     const My_Game& state, int player_index, std::mt19937& rng
//   );
//
//   sample_state returns one position that player_index cannot tell apart from
//   `state`. It shuffles whatever player_index cannot see. The agent draws many
//   such positions, searches each one, and votes.
//
// A missing function reads as "no matching function for call to
// evaluate_state(...)", reported at the line inside the search that calls it.

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

// The same, applying `choice` instead of the game's own pending one. A search
// copies a position per child, and every child waits on the same choice — so
// it is taken out of the parent once and applied to each child from here,
// instead of riding along in every copy. A Choice holds two std::functions,
// which a copy has to allocate for.
void resolve_choice(Game& game, const Choice& choice, int index);

// Forward declaration; defined in agent.h.
struct Agent;

void game_loop(
  Game& game, Agent& agent, std::function<void(Game&)> callback = nullptr
);
bool game_frame(Game& game, Agent& agent);
