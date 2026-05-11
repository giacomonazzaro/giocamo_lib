#include "models.h"

#include <game/game.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace tressette {

int strength(int rank) {
  // Tressette ordering: 3 > 2 > 1 > 10 > 9 > 8 > 7 > 6 > 5 > 4.
  switch (rank) {
    case 3: return 9;
    case 2: return 8;
    case 1: return 7;
    case 10: return 6;
    case 9: return 5;
    case 8: return 4;
    case 7: return 3;
    case 6: return 2;
    case 5: return 1;
    case 4: return 0;
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

// Same DEF_INT_VEC pattern as gods: properties that return a reference to
// the underlying std::vector<int> so Python list ops mutate it in place, and
// accept any iterable as a setter.
#define DEF_INT_VEC(CLS, NAME, MEMBER)                                \
  def_prop_rw(                                                        \
    NAME,                                                             \
    [](CLS& self) -> std::vector<int>& { return self.MEMBER; },       \
    [](CLS& self, nb::list v) {                                       \
      self.MEMBER.clear();                                            \
      for (auto item : v) self.MEMBER.push_back(nb::cast<int>(item)); \
    },                                                                \
    nb::rv_policy::reference_internal                                 \
  )

void bind_models(nb::module_& m) {
  nb::bind_vector<std::vector<int>>(m, "IntVector");

  nb::enum_<Suit>(m, "Suit")
    .value("COPPE", Suit::COPPE)
    .value("DENARI", Suit::DENARI)
    .value("SPADE", Suit::SPADE)
    .value("BASTONI", Suit::BASTONI)
    .export_values();

  nb::class_<Card>(m, "Card")
    .def(nb::init<>())
    .def(
      "__init__",
      [](Card* c, int id, int rank, Suit suit) {
        new (c) Card();
        c->id   = id;
        c->rank = rank;
        c->suit = suit;
      },
      "id"_a,
      "rank"_a,
      "suit"_a
    )
    .def_rw("id", &Card::id)
    .def_rw("rank", &Card::rank)
    .def_rw("suit", &Card::suit);

  nb::class_<Player>(m, "Player")
    .def(nb::init<>())
    .def(
      "__init__",
      [](
        Player*          p,
        std::string      name,
        std::vector<int> hand,
        std::vector<int> tricks_won
      ) {
        new (p) Player();
        p->name       = name;
        p->hand       = hand;
        p->tricks_won = tricks_won;
      },
      "name"_a = "",
      nb::kw_only(),
      "hand"_a       = std::vector<int>{},
      "tricks_won"_a = std::vector<int>{}
    )
    .def_rw("name", &Player::name)
    .DEF_INT_VEC(Player, "hand", hand)
    .DEF_INT_VEC(Player, "tricks_won", tricks_won);

  // Game / Choice / Choose_* / action_options come from game._game; this
  // module only adds the tressette-specific Game_State subclass.
  nb::class_<Game_State, Game>(m, "Game_State")
    .def(nb::init<>())
    .def_rw("all_cards", &Game_State::all_cards)
    .def_rw("players", &Game_State::players)
    .DEF_INT_VEC(Game_State, "stock", stock)
    .DEF_INT_VEC(Game_State, "trick", trick)
    .def_rw("trick_leader", &Game_State::trick_leader)
    .def_rw("current_player", &Game_State::current_player)
    .def_rw("last_trick_winner", &Game_State::last_trick_winner)
    .def_rw("game_over", &Game_State::game_over)
    .def_rw("on_cards_changed", &Game_State::on_cards_changed)
    .def("switch_turn", &Game_State::switch_turn);
}
