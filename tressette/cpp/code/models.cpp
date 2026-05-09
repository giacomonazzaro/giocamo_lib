#include "models.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>

#include <game_cpp/game.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace tressette {

int strength(int rank) {
  // Tressette ordering: 3 > 2 > 1 > 10 > 9 > 8 > 7 > 6 > 5 > 4.
  switch (rank) {
    case 3:  return 9;
    case 2:  return 8;
    case 1:  return 7;
    case 10: return 6;
    case 9:  return 5;
    case 8:  return 4;
    case 7:  return 3;
    case 6:  return 2;
    case 5:  return 1;
    case 4:  return 0;
  }
  return 0;
}

int card_thirds(int rank) {
  if (rank == 1) return 3;
  if (rank == 2 || rank == 3 || rank == 8 || rank == 9 || rank == 10) return 1;
  return 0;
}

}  // namespace tressette

using namespace tressette;

// Same DEF_INT_VEC pattern as gods_cpp: properties that return a reference to
// the underlying std::vector<int> so Python list ops mutate it in place, and
// accept any iterable as a setter.
#define DEF_INT_VEC(CLS, NAME, MEMBER)                                       \
  def_prop_rw(                                                                \
    NAME,                                                                     \
    [](CLS& self) -> std::vector<int>& { return self.MEMBER; },               \
    [](CLS& self, nb::list v) {                                               \
      self.MEMBER.clear();                                                    \
      for (auto item : v) self.MEMBER.push_back(nb::cast<int>(item));         \
    },                                                                        \
    nb::rv_policy::reference_internal                                         \
  )

void bind_models(nb::module_& m) {
  nb::bind_vector<std::vector<int>>(m, "IntVector");

  nb::enum_<Suit>(m, "Suit")
    .value("COPPE",   Suit::COPPE)
    .value("DENARI",  Suit::DENARI)
    .value("SPADE",   Suit::SPADE)
    .value("BASTONI", Suit::BASTONI)
    .export_values();

  nb::class_<Card>(m, "Card")
    .def(nb::init<>())
    .def("__init__",
         [](Card* c, int id, int rank, Suit suit) {
           new (c) Card();
           c->id   = id;
           c->rank = rank;
           c->suit = suit;
         },
         "id"_a, "rank"_a, "suit"_a)
    .def_rw("id", &Card::id)
    .def_rw("rank", &Card::rank)
    .def_rw("suit", &Card::suit);

  nb::class_<Player>(m, "Player")
    .def(nb::init<>())
    .def("__init__",
         [](Player* p, std::string name, std::vector<int> hand,
            std::vector<int> tricks_won) {
           new (p) Player();
           p->name       = name;
           p->hand       = hand;
           p->tricks_won = tricks_won;
         },
         "name"_a = "",
         nb::kw_only(),
         "hand"_a       = std::vector<int>{},
         "tricks_won"_a = std::vector<int>{})
    .def_rw("name", &Player::name)
    .DEF_INT_VEC(Player, "hand", hand)
    .DEF_INT_VEC(Player, "tricks_won", tricks_won);

  // game_cpp choice variants — bound here so Python isinstance() works against
  // the result of choice.actions(state). Tressette uses Choose_Card only, but
  // we bind all four to keep the shim symmetric with gods.
  nb::class_<Choose_Card>(m, "Choose_Card")
    .def(nb::init<>())
    .def_prop_ro("targets",
                 [](const Choose_Card& c) { return c.targets; })
    .def_rw("up_to", &Choose_Card::up_to);

  nb::class_<Choose_Cards>(m, "Choose_Cards")
    .def(nb::init<>())
    .def_prop_ro("targets",
                 [](const Choose_Cards& c) { return c.targets; })
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
    .def("actions",
         [](Choice& c, Game_State& g) -> nb::object {
           Choose ch = c.actions(g);
           // Return the variant alternative directly so Python isinstance works.
           return std::visit([](auto&& v) -> nb::object { return nb::cast(v); }, ch);
         },
         "state"_a)
    .def("resolve",
         [](Choice& c, Game_State& g, int index) { return c.resolve(g, index); },
         "state"_a, "index"_a);

  // action_options(choose) — enumerate selectable options as a Python list.
  // Tressette only ever produces Choose_Card; the others are no-ops kept for
  // symmetry with gods.
  m.def("action_options",
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
        "choose"_a);

  nb::class_<Game_State>(m, "Game_State")
    .def(nb::init<>())
    .def_rw("all_cards", &Game_State::all_cards)
    .def_rw("players", &Game_State::players)
    .DEF_INT_VEC(Game_State, "stock", stock)
    .DEF_INT_VEC(Game_State, "trick", trick)
    .def_rw("trick_leader", &Game_State::trick_leader)
    .def_rw("current_player", &Game_State::current_player)
    .def_rw("last_trick_winner", &Game_State::last_trick_winner)
    .def_rw("game_over", &Game_State::game_over)
    .def_rw("choices", &Game_State::choices)
    .def_rw("on_cards_changed", &Game_State::on_cards_changed)
    .def("is_game_over", &Game_State::is_game_over)
    .def("next_choice", &Game_State::next_choice)
    .def("switch_turn", &Game_State::switch_turn);
}
