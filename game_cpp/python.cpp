// nanobind bindings for the game_cpp/ generic engine.
//
// Exposes Game, Choice, Choose_*, Agent (with a Python-override bridge),
// Agent_Random, Agent_Duel, action_options(_count), game_frame, game_loop,
// resolve_choice — i.e. everything a concrete game needs from the framework.
//
// Each game (tressette, gods, ...) keeps its own nanobind module for its
// game-specific types and pulls these in by importing `game._game_cpp` from
// its NB_MODULE. Python sees a single class hierarchy: Game_State extends
// Game, the AI subclasses Agent, etc.
#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>
#include <nanobind/trampoline.h>

#include <cstdint>

#include "agent.h"
#include "game.h"

namespace nb = nanobind;
using namespace nb::literals;

namespace {

// Bridges C++ virtual dispatch on Agent to Python overrides. When C++ code
// calls agent.choose_action(...) on an Agent& whose underlying object is a
// Python subclass of the bound `Agent`, the trampoline forwards the call to
// the Python override. Without it, C++ has no way to reach a method defined
// in Python.
struct PyAgent : Agent {
  NB_TRAMPOLINE(Agent, 2);

  int choose_action(Game& state, const Choice& choice) override {
    NB_OVERRIDE_PURE(choose_action, state, choice);
  }
  void message(const std::string& msg) override { NB_OVERRIDE(message, msg); }
};

}  // namespace

NB_MODULE(_game_cpp, m) {
  // Forward-declare Game so Choice (which holds Game&-taking callbacks) can
  // refer to it; methods that return optional<Choice> get added below.
  auto game_cls = nb::class_<Game>(m, "Game");

  nb::class_<Choose_Card>(m, "Choose_Card")
    .def(nb::init<>())
    .def_prop_ro("targets", [](const Choose_Card& c) { return c.targets; })
    .def_rw("up_to", &Choose_Card::up_to);

  nb::class_<Choose_Cards>(m, "Choose_Cards")
    .def(nb::init<>())
    .def_prop_ro("targets", [](const Choose_Cards& c) { return c.targets; })
    .def_rw("count", &Choose_Cards::count)
    .def_rw("up_to", &Choose_Cards::up_to);

  nb::class_<Choose_Option>(m, "Choose_Option")
    .def(nb::init<>())
    .def_rw("targets", &Choose_Option::targets);

  nb::class_<Choose_Options>(m, "Choose_Options")
    .def(nb::init<>())
    .def_rw("targets", &Choose_Options::targets)
    .def_rw("count", &Choose_Options::count)
    .def_rw("up_to", &Choose_Options::up_to);

  nb::class_<Choice>(m, "Choice")
    .def(nb::init<>())
    .def_rw("player_index", &Choice::player_index)
    .def_rw("description", &Choice::description)
    .def_rw("text_description", &Choice::text_description)
    .def(
      "actions",
      [](Choice& c, Game& g) -> nb::object {
        Choose ch = c.actions(g);
        // Return the variant alternative directly so Python isinstance works.
        return std::visit(
          [](auto&& v) -> nb::object { return nb::cast(v); }, ch
        );
      },
      "state"_a
    )
    .def(
      "resolve",
      [](Choice& c, Game& g, int index) { return c.resolve(g, index); },
      "state"_a,
      "index"_a
    );

  // Now that Choice is bound, finish the Game class.
  game_cls.def("is_game_over", &Game::is_game_over)
    .def("next_choice", &Game::next_choice)
    .def_rw("choices", &Game::choices);

  m.def("action_options_count", &action_options_count, "choose"_a);

  // Default enumerator: returns int targets for Choose_Card and string
  // targets for Choose_Option. Game-specific bindings can override or extend
  // for richer Choose_Cards / Choose_Options support.
  m.def(
    "action_options",
    [](nb::object choose) -> nb::list {
      nb::list result;
      if (nb::isinstance<Choose_Card>(choose)) {
        const Choose_Card& c = nb::cast<const Choose_Card&>(choose);
        for (int t : c.targets) result.append(t);
      } else if (nb::isinstance<Choose_Option>(choose)) {
        const Choose_Option& c = nb::cast<const Choose_Option&>(choose);
        for (const auto& s : c.targets) result.append(s);
      }
      return result;
    },
    "choose"_a
  );

  nb::class_<Agent, PyAgent>(m, "Agent")
    .def(nb::init<>())
    .def("choose_action", &Agent::choose_action, "state"_a, "choice"_a)
    .def("message", &Agent::message, "msg"_a);

  nb::class_<Agent_Random, Agent>(m, "Agent_Random")
    .def(nb::init<>())
    .def(nb::init<std::uint32_t>(), "seed"_a);

  // Agent_Duel multiplexes by choice.player_index. The keep_alive annotations
  // ensure the inner Agent objects (often Python subclasses of Agent) stay
  // alive as long as the Agent_Duel does.
  nb::class_<Agent_Duel, Agent>(m, "Agent_Duel")
    .def(
      nb::init<Agent*, Agent*, bool>(),
      "agent_0"_a,
      "agent_1"_a,
      "swap"_a,
      nb::keep_alive<1, 2>(),
      nb::keep_alive<1, 3>()
    );

  m.def("game_frame", &game_frame, "state"_a, "agent"_a, "choice"_a);
  m.def("resolve_choice", &resolve_choice, "state"_a, "choice"_a, "index"_a);
  m.def(
    "game_loop",
    [](Game& g, Agent& a) { game_loop(g, a, nullptr); },
    "state"_a,
    "agent"_a
  );
}
