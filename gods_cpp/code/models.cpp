#include "models.h"

#include <algorithm>
#include <numeric>
#include <sstream>

#include <game_cpp/game.h>

#include "gameplay.h"  // For make_main_choice / make_claim_choice in next_choice.

#ifdef GODS_BUILD_PYTHON
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>
using namespace nb::literals;
#endif

// ---- Card_Id packing ----

static int area_to_code(const std::string& area) {
  if (area == "deck")    return area_code::DECK;
  if (area == "hand")    return area_code::HAND;
  if (area == "discard") return area_code::DISCARD;
  if (area == "wonders") return area_code::WONDERS;
  if (area == "people")  return area_code::PEOPLE;
  return area_code::NONE;
}

static std::string code_to_area(int code) {
  switch (code) {
    case area_code::DECK:    return "deck";
    case area_code::HAND:    return "hand";
    case area_code::DISCARD: return "discard";
    case area_code::WONDERS: return "wonders";
    case area_code::PEOPLE:  return "people";
    default:                 return "none";
  }
}

int pack_card_id(const Card_Id& cid) {
  if (Card_Id::is_null(cid)) return 0;  // Null sentinel.
  int idx   = cid.card_index & 0xFFFFFF;
  int area  = (area_to_code(cid.area) & 0xF) << 24;
  int owner = ((cid.owner_index + 1) & 0xF) << 28;  // -1 -> 0, 0 -> 1, 1 -> 2.
  return idx | area | owner;
}

Card_Id unpack_card_id(int packed) {
  if (packed == 0) return Card_Id::null();
  Card_Id cid;
  cid.card_index  = packed & 0xFFFFFF;
  cid.area        = code_to_area((packed >> 24) & 0xF);
  cid.owner_index = ((packed >> 28) & 0xF) - 1;
  return cid;
}

// ---- Game_State methods ----

std::vector<Card_Id> Game_State::peoples_ids() const {
  std::vector<Card_Id> out;
  for (int pid : peoples) {
    out.push_back(Card_Id{"people", pid, all_cards[pid].owner});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

std::vector<Card_Id> Game_State::wonders_of(int player_index) const {
  std::vector<Card_Id> out;
  for (int wid : players[player_index].wonders) {
    out.push_back(Card_Id{"wonders", wid, player_index});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

std::vector<Card_Id> Game_State::discard_of(int player_index) const {
  std::vector<Card_Id> out;
  for (int did : players[player_index].discard) {
    out.push_back(Card_Id{"discard", did, player_index});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

std::vector<Card_Id> Game_State::hand_of(int player_index) const {
  std::vector<Card_Id> out;
  for (int hid : players[player_index].hand) {
    out.push_back(Card_Id{"hand", hid, player_index});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

std::vector<Card_Id> Game_State::card_list(int player_id, const std::string& area) const {
  if (player_id == -1) {
    auto a = card_list(0, area);
    auto b = card_list(1, area);
    a.insert(a.end(), b.begin(), b.end());
    std::sort(a.begin(), a.end(),
              [](const Card_Id& x, const Card_Id& y) { return x.card_index < y.card_index; });
    return a;
  }
  const Player& p = players[player_id];
  const std::vector<int>* src = nullptr;
  if      (area == "hand")    src = &p.hand;
  else if (area == "wonders") src = &p.wonders;
  else if (area == "discard") src = &p.discard;
  else if (area == "deck")    src = &p.deck;
  std::vector<Card_Id> out;
  if (src) {
    for (int cid : *src) out.push_back(Card_Id{area, cid, player_id});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

int Game_State::effective_power(int card_id) const {
  const Card& card = all_cards[card_id];
  int power = card.power + card.counters;
  for (const Player& player : players) {
    for (int wid : player.wonders) {
      // const_cast: power_modifier may be a virtual called on a non-const
      // Game_State&; semantically it must not mutate state during a query.
      power = const_cast<Card&>(all_cards[wid]).power_modifier(
        const_cast<Game_State&>(*this), card, power);
    }
  }
  if (power < 0) power = 0;
  return power;
}

std::optional<Choice> Game_State::next_choice() {
  while (!game_over) {
    if (!choices.empty()) {
      Choice c = std::move(choices.front());
      choices.erase(choices.begin());
      Choose actions = c.actions(*this);
      if (action_options_count(actions) == 0) continue;
      return c;
    }

    if (current_phase == "start") {
      for (int wid : active_player().wonders) {
        auto extra = all_cards[wid].on_turn_start(*this);
        for (auto& ch : extra) choices.push_back(std::move(ch));
      }
      current_phase = "main";
    } else if (current_phase == "main") {
      choices.push_back(make_main_choice(*this));
    } else if (current_phase == "post-play") {
      current_phase = "claim";
    } else if (current_phase == "post-pass-effects") {
      Player& player = active_player();
      if (player.deck.empty()) {
        game_over = true;
        continue;
      }
      auto extra = draw_card(*this, current_player);
      for (auto& ch : extra) choices.push_back(std::move(ch));
      current_phase = "post-pass-draw";
    } else if (current_phase == "post-pass-draw") {
      current_phase = "claim";
    } else if (current_phase == "claim") {
      auto claim = make_claim_choice(*this);
      if (claim) choices.push_back(std::move(*claim));
      current_phase = "end";
    } else if (current_phase == "end") {
      for (int wid : active_player().wonders) {
        auto extra = all_cards[wid].on_turn_end(*this);
        for (auto& ch : extra) choices.push_back(std::move(ch));
      }
      switch_turn();
      current_phase = "start";
    }
  }
  return std::nullopt;
}

// ---- Bindings ----

// Helper macro: emit a def_prop_rw for a std::vector<int> field that returns
// the underlying vector by reference (so list.pop/append mutate C++ state) and
// accepts any Python iterable as a setter. The cast lambdas need an explicit
// host type so nanobind can introspect the signature.
#ifdef GODS_BUILD_PYTHON

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
  // std::vector<int> exposed as a Python sequence with by-reference semantics
  // (so list.pop()/append() on a property mutates the underlying vector).
  nb::bind_vector<std::vector<int>>(m, "IntVector");

  // Enums.
  nb::enum_<Card_Type>(m, "Card_Type")
    .value("WONDER", Card_Type::WONDER)
    .value("EVENT",  Card_Type::EVENT)
    .value("PEOPLE", Card_Type::PEOPLE)
    .export_values();

  nb::enum_<Card_Color>(m, "Card_Color")
    .value("GREEN",  Card_Color::GREEN)
    .value("BLUE",   Card_Color::BLUE)
    .value("RED",    Card_Color::RED)
    .value("YELLOW", Card_Color::YELLOW)
    .export_values();

  // Card_Id — frozen-style: equality, hash, and a static null/is_null.
  nb::class_<Card_Id>(m, "Card_Id")
    .def(nb::init<>())
    .def("__init__",
         [](Card_Id* c, std::string area, int card_index, int owner_index) {
           new (c) Card_Id();
           c->area = area;
           c->card_index = card_index;
           c->owner_index = owner_index;
         },
         "area"_a, "card_index"_a, "owner_index"_a)
    .def_rw("area", &Card_Id::area)
    .def_rw("card_index", &Card_Id::card_index)
    .def_rw("owner_index", &Card_Id::owner_index)
    .def("__eq__",
         [](const Card_Id& a, const Card_Id& b) { return a == b; })
    .def("__ne__",
         [](const Card_Id& a, const Card_Id& b) { return !(a == b); })
    .def("__hash__",
         [](const Card_Id& c) {
           size_t h = std::hash<std::string>{}(c.area);
           h = h * 31 + std::hash<int>{}(c.card_index);
           h = h * 31 + std::hash<int>{}(c.owner_index);
           return h;
         })
    .def("__repr__",
         [](const Card_Id& c) {
           std::ostringstream ss;
           ss << "Card_Id(area=" << c.area << ", card_index=" << c.card_index
              << ", owner_index=" << c.owner_index << ")";
           return ss.str();
         })
    .def_static("null", &Card_Id::null)
    .def_static("is_null", &Card_Id::is_null);

  // Card — runtime card state.
  nb::class_<Card>(m, "Card")
    .def(nb::init<>())
    .def("__init__",
         [](Card* c, int id, Card_Type card_type, Card_Color color, int power,
            int counters, bool destroyed, int owner) {
           new (c) Card();
           c->id        = id;
           c->card_type = card_type;
           c->color     = color;
           c->power     = power;
           c->counters  = counters;
           c->destroyed = destroyed;
           c->owner     = owner;
         },
         "id"_a, "card_type"_a, "color"_a, "power"_a,
         nb::kw_only(),
         "counters"_a  = 0,
         "destroyed"_a = false,
         "owner"_a     = -1)
    .def_rw("id", &Card::id)
    .def_rw("card_type", &Card::card_type)
    .def_rw("color", &Card::color)
    .def_rw("power", &Card::power)
    .def_rw("counters", &Card::counters)
    .def_rw("destroyed", &Card::destroyed)
    .def_rw("owner", &Card::owner)
    // Expose .name lookup through the global card_designs registry — agents and
    // UI use card.name for display.
    .def_prop_ro("name",
                 [](const Card& c) {
                   if (c.id >= 0 && c.id < (int)card_designs.size()
                       && card_designs[c.id]) {
                     return card_designs[c.id]->name;
                   }
                   return std::string("");
                 });

  // Player — owns deck/hand/discard/wonders as int lists.
  nb::class_<Player>(m, "Player")
    .def(nb::init<>())
    .def("__init__",
         [](Player* p, std::string name, std::vector<int> deck,
            std::vector<int> hand, std::vector<int> discard,
            std::vector<int> wonders) {
           new (p) Player();
           p->name    = name;
           p->deck    = deck;
           p->hand    = hand;
           p->discard = discard;
           p->wonders = wonders;
         },
         "name"_a = "",
         nb::kw_only(),
         "deck"_a    = std::vector<int>{},
         "hand"_a    = std::vector<int>{},
         "discard"_a = std::vector<int>{},
         "wonders"_a = std::vector<int>{})
    .def_rw("name", &Player::name)
    .DEF_INT_VEC(Player, "deck", deck)
    .DEF_INT_VEC(Player, "hand", hand)
    .DEF_INT_VEC(Player, "discard", discard)
    .DEF_INT_VEC(Player, "wonders", wonders);

  // Card_Design — base class; subclasses are not bound individually since
  // Python only ever reads the base attributes (name, effect, etc.).
  nb::class_<Card_Design>(m, "Card_Design")
    .def_rw("id", &Card_Design::id)
    .def_rw("name", &Card_Design::name)
    .def_rw("card_type", &Card_Design::card_type)
    .def_rw("color", &Card_Design::color)
    .def_rw("effect", &Card_Design::effect);

  // ---- game_cpp choice types ----
  // Bound here so Python (agent_ui.py, agent_terminal.py) can isinstance-check
  // them. Internal int targets are wrapped as Card_Id by the action_options
  // helper below.

  nb::class_<Choose_Card>(m, "Choose_Card")
    .def(nb::init<>())
    .def_prop_ro("targets",
                 [](const Choose_Card& c) {
                   std::vector<Card_Id> out;
                   for (int t : c.targets) out.push_back(unpack_card_id(t));
                   return out;
                 })
    .def_rw("up_to", &Choose_Card::up_to);

  nb::class_<Choose_Cards>(m, "Choose_Cards")
    .def(nb::init<>())
    .def_prop_ro("targets",
                 [](const Choose_Cards& c) {
                   std::vector<Card_Id> out;
                   for (int t : c.targets) out.push_back(unpack_card_id(t));
                   return out;
                 })
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

  // Choice — exposes its callable hooks as Python methods.
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

  // action_options(choose) → list of selectable options. Mirrors the helper in
  // game/game.py so Python code that imports it from `game.game` keeps working.
  m.def("action_options",
        [](nb::object choose) -> nb::list {
          // Dispatch on Python type since Choose is a variant.
          nb::list result;
          if (nb::isinstance<Choose_Card>(choose)) {
            const Choose_Card& c = nb::cast<const Choose_Card&>(choose);
            for (int t : c.targets) result.append(unpack_card_id(t));
          } else if (nb::isinstance<Choose_Option>(choose)) {
            const Choose_Option& c = nb::cast<const Choose_Option&>(choose);
            for (const auto& s : c.targets) result.append(s);
          } else if (nb::isinstance<Choose_Cards>(choose)) {
            const Choose_Cards& c = nb::cast<const Choose_Cards&>(choose);
            std::vector<Card_Id> targets;
            for (int t : c.targets) targets.push_back(unpack_card_id(t));
            auto combos = all_combinations(targets, c.count, c.up_to);
            for (const auto& combo : combos) {
              nb::list py_combo;
              for (const auto& cid : combo) py_combo.append(cid);
              result.append(nb::tuple(py_combo));
            }
          } else if (nb::isinstance<Choose_Options>(choose)) {
            const Choose_Options& c = nb::cast<const Choose_Options&>(choose);
            // Enumerate combinations as tuples of strings.
            int n = (int)c.targets.size();
            int count = c.count;
            auto append_combo = [&](const std::vector<int>& idx) {
              nb::list py_combo;
              for (int i : idx) py_combo.append(c.targets[i]);
              result.append(nb::tuple(py_combo));
            };
            if (c.up_to) {
              for (int k = 0; k <= std::min(count, n); ++k) {
                if (k == 0) {
                  result.append(nb::tuple(nb::list()));
                  continue;
                }
                std::vector<bool> mask(n, false);
                std::fill(mask.end() - k, mask.end(), true);
                do {
                  std::vector<int> idx;
                  for (int i = 0; i < n; ++i) if (mask[i]) idx.push_back(i);
                  append_combo(idx);
                } while (std::next_permutation(mask.begin(), mask.end()));
              }
            } else {
              if (n <= count) {
                std::vector<int> idx(n);
                std::iota(idx.begin(), idx.end(), 0);
                append_combo(idx);
              } else {
                std::vector<bool> mask(n, false);
                std::fill(mask.end() - count, mask.end(), true);
                do {
                  std::vector<int> idx;
                  for (int i = 0; i < n; ++i) if (mask[i]) idx.push_back(i);
                  append_combo(idx);
                } while (std::next_permutation(mask.begin(), mask.end()));
              }
            }
          }
          return result;
        },
        "choose"_a);

  // ---- Game_State ----
  nb::class_<Game_State>(m, "Game_State")
    .def(nb::init<>())
    .def_rw("all_cards", &Game_State::all_cards)
    .def_rw("players", &Game_State::players)
    .DEF_INT_VEC(Game_State, "peoples", peoples)
    .def_rw("current_player", &Game_State::current_player)
    .def_rw("current_phase", &Game_State::current_phase)
    .DEF_INT_VEC(Game_State, "shared_deck", shared_deck)
    .def_rw("game_over", &Game_State::game_over)
    .def_rw("choices", &Game_State::choices)
    .def_rw("on_cards_changed", &Game_State::on_cards_changed)
    .def("is_game_over", &Game_State::is_game_over)
    .def("next_choice", &Game_State::next_choice)
    .def("active_player", &Game_State::active_player, nb::rv_policy::reference_internal)
    .def("opponent", &Game_State::opponent, nb::rv_policy::reference_internal)
    .def("peoples_ids", &Game_State::peoples_ids)
    .def("wonders", &Game_State::wonders_of, "player_index"_a)
    .def("discard", &Game_State::discard_of, "player_index"_a)
    .def("hand", &Game_State::hand_of, "player_index"_a)
    .def("switch_turn", &Game_State::switch_turn)
    .def("get_card", &Game_State::get_card, "card_id"_a, nb::rv_policy::reference_internal)
    .def("card_list",
         [](const Game_State& g, nb::object player_id, const std::string& area) {
           int pid = player_id.is_none() ? -1 : nb::cast<int>(player_id);
           return g.card_list(pid, area);
         },
         "player_id"_a, "area"_a)
    .def("effective_power", &Game_State::effective_power, "card_id"_a)
    .def("owner", &Game_State::owner, "card_id"_a);

  // Module-level card_designs — a list-like view returning the existing
  // Card_Design objects by reference.
  m.def("get_card_designs",
        []() {
          nb::list out;
          for (const auto& d : card_designs) {
            if (d) out.append(nb::cast(d.get(), nb::rv_policy::reference));
          }
          return out;
        });
  // Simpler: a Python module attribute that gives a fresh list view each time.
  // Python code uses `from gods.models import card_designs` so we need an attr.
  // Bind it as a property on the module via a callable: `card_designs` in
  // Python returns a list. Done in the shim instead — see gods/models.py.
}

#endif // GODS_BUILD_PYTHON
