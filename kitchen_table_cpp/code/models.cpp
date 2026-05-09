#include "models.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>

#include <sstream>

namespace nb = nanobind;
using namespace nb::literals;

Table_State::Table_State()
    : is_drop_card_allowed([](int, int, int) { return true; }) {}

std::optional<std::tuple<int, int, int>> Table_State::poll_dropped_card() {
  auto result  = dropped_card;
  dropped_card = std::nullopt;
  return result;
}

// Proxy types that hold a raw pointer to a vector and a Python reference to the
// owner, giving true element reference semantics from Python without copying on
// __getitem__.
struct Stacks_Ref {
  std::vector<Stack>* vec;
  nb::object          owner;
};
struct Cards_Ref {
  std::vector<Card>* vec;
  nb::object         owner;
};

void bind_models(nb::module_& m) {
  // std::vector<int> binds as a by-reference Python sequence.
  nb::bind_vector<std::vector<int>>(m, "IntVector");

  // KT_Rectangle bound as "Rectangle" so pyray's cffi accepts it as a Rectangle
  // argument.
  nb::class_<KT_Rectangle>(m, "Rectangle")
    .def(nb::init<>())
    .def(
      "__init__",
      [](KT_Rectangle* r, float x, float y, float width, float height) {
        new (r) KT_Rectangle();
        r->x      = x;
        r->y      = y;
        r->width  = width;
        r->height = height;
      },
      "x"_a      = 0.0f,
      "y"_a      = 0.0f,
      "width"_a  = 0.0f,
      "height"_a = 0.0f
    )
    .def_rw("x", &KT_Rectangle::x)
    .def_rw("y", &KT_Rectangle::y)
    .def_rw("width", &KT_Rectangle::width)
    .def_rw("height", &KT_Rectangle::height)
    .def(
      "__iter__",
      [](const KT_Rectangle& r) {
        return nb::make_tuple(r.x, r.y, r.width, r.height).attr("__iter__")();
      }
    )
    .def("__len__", [](const KT_Rectangle&) { return 4; })
    .def("__repr__", [](const KT_Rectangle& r) {
      std::ostringstream ss;
      ss << "Rectangle(x=" << r.x << ", y=" << r.y << ", width=" << r.width
         << ", height=" << r.height << ")";
      return ss.str();
    });

  // Thing — base visual entity with optional draw callback.
  nb::class_<Thing>(m, "Thing")
    .def(nb::init<>())
    .def(
      "__init__",
      [](
        Thing*                      t,
        int                         id,
        std::string                 image_path,
        float                       x,
        float                       y,
        int                         rotation,
        std::function<void(Thing&)> draw_callback
      ) {
        new (t) Thing();
        t->id            = id;
        t->image_path    = image_path;
        t->x             = x;
        t->y             = y;
        t->rotation      = rotation;
        t->draw_callback = std::move(draw_callback);
      },
      "id"_a = 0,
      nb::kw_only(),
      "image_path"_a    = "",
      "x"_a             = 0.0f,
      "y"_a             = 0.0f,
      "rotation"_a      = 0,
      "draw_callback"_a = nb::none()
    )
    .def_rw("id", &Thing::id)
    .def_rw("image_path", &Thing::image_path)
    .def_rw("x", &Thing::x)
    .def_rw("y", &Thing::y)
    .def_rw("rotation", &Thing::rotation)
    .def_rw("draw_callback", &Thing::draw_callback);

  // Card — inherits all Thing fields.
  nb::class_<Card, Thing>(m, "Card")
    .def(nb::init<>())
    .def(
      "__init__",
      [](
        Card*                       c,
        int                         id,
        std::string                 image_path,
        float                       x,
        float                       y,
        int                         rotation,
        std::function<void(Thing&)> draw_callback
      ) {
        new (c) Card();
        c->id            = id;
        c->image_path    = image_path;
        c->x             = x;
        c->y             = y;
        c->rotation      = rotation;
        c->draw_callback = std::move(draw_callback);
      },
      "id"_a = 0,
      nb::kw_only(),
      "image_path"_a    = "",
      "x"_a             = 0.0f,
      "y"_a             = 0.0f,
      "rotation"_a      = 0,
      "draw_callback"_a = nb::none()
    );

  // Stack — ordered pile of cards with layout parameters.
  // cards is exposed as a by-reference IntVector so Python mutations are
  // visible in C++.
  nb::class_<Stack>(m, "Stack")
    .def(
      "__init__",
      [](
        Stack*      s,
        nb::object  rect,
        nb::list    cards,
        float       spread_x,
        float       spread_y,
        bool        face_up,
        std::string name,
        float       depth,
        int         capacity
      ) {
        new (s) Stack();
        s->rect.x      = nb::cast<float>(rect.attr("x"));
        s->rect.y      = nb::cast<float>(rect.attr("y"));
        s->rect.width  = nb::cast<float>(rect.attr("width"));
        s->rect.height = nb::cast<float>(rect.attr("height"));
        for (auto item : cards) s->cards.push_back(nb::cast<int>(item));
        s->spread_x = spread_x;
        s->spread_y = spread_y;
        s->face_up  = face_up;
        s->name     = name;
        s->depth    = depth;
        s->capacity = capacity;
      },
      "rect"_a,
      nb::kw_only(),
      "cards"_a    = nb::list(),
      "spread_x"_a = 0.0f,
      "spread_y"_a = 0.0f,
      "face_up"_a  = true,
      "name"_a     = "",
      "depth"_a    = 0.0f,
      "capacity"_a = -1
    )
    .def_prop_rw(
      "rect",
      [](const Stack& s) -> KT_Rectangle { return s.rect; },
      [](Stack& s, nb::object r) {
        s.rect.x      = nb::cast<float>(r.attr("x"));
        s.rect.y      = nb::cast<float>(r.attr("y"));
        s.rect.width  = nb::cast<float>(r.attr("width"));
        s.rect.height = nb::cast<float>(r.attr("height"));
      }
    )
    // Getter returns a by-reference view; setter accepts any Python sequence.
    .def_prop_rw(
      "cards",
      [](Stack& s) -> std::vector<int>& { return s.cards; },
      [](Stack& s, nb::list v) {
        s.cards.clear();
        for (auto item : v) s.cards.push_back(nb::cast<int>(item));
      },
      nb::rv_policy::reference_internal
    )
    .def_rw("spread_x", &Stack::spread_x)
    .def_rw("spread_y", &Stack::spread_y)
    .def_rw("face_up", &Stack::face_up)
    .def_rw("name", &Stack::name)
    .def_rw("depth", &Stack::depth)
    .def_rw("capacity", &Stack::capacity);

  // CardVector proxy — __getitem__ returns by copy (read-only access is
  // sufficient since Python does not mutate individual card fields; C++ handles
  // card position updates).
  nb::class_<Cards_Ref>(m, "CardVector")
    .def("__len__", [](const Cards_Ref& r) { return r.vec->size(); })
    .def(
      "__getitem__",
      [](Cards_Ref& r, int i) -> Card& {
        int n = (int)r.vec->size();
        if (i < 0) i += n;
        if (i < 0 || i >= n) throw nb::index_error("index out of range");
        return (*r.vec)[i];
      },
      nb::rv_policy::reference_internal
    )
    .def(
      "__setitem__",
      [](Cards_Ref& r, int i, const Card& v) {
        int n = (int)r.vec->size();
        if (i < 0) i += n;
        (*r.vec)[i] = v;
      }
    )
    .def("append", [](Cards_Ref& r, const Card& c) { r.vec->push_back(c); })
    .def("__iter__", [](Cards_Ref& r) {
      nb::list l;
      for (const auto& c : *r.vec) l.append(nb::cast(c));
      return l.attr("__iter__")();
    });

  // StackVector proxy — delegates __getitem__ to owner._get_stack() which
  // returns a true Stack& reference with Table_State as parent, giving correct
  // nanobind reference semantics.
  nb::class_<Stacks_Ref>(m, "StackVector")
    .def("__len__", [](const Stacks_Ref& r) { return r.vec->size(); })
    .def(
      "__getitem__",
      [](Stacks_Ref& r, int i) {
        int n = (int)r.vec->size();
        if (i < 0) i += n;
        if (i < 0 || i >= n) throw nb::index_error("index out of range");
        // Delegate to Table_State._get_stack so the returned Stack& is owned by
        // Table_State.
        return r.owner.attr("get_stack")(i);
      }
    )
    .def(
      "__setitem__",
      [](Stacks_Ref& r, int i, const Stack& v) {
        int n = (int)r.vec->size();
        if (i < 0) i += n;
        (*r.vec)[i] = v;
      }
    )
    .def("append", [](Stacks_Ref& r, const Stack& s) { r.vec->push_back(s); })
    .def("__iter__", [](Stacks_Ref& r) {
      nb::list l;
      for (const auto& s : *r.vec) l.append(nb::cast(s));
      return l.attr("__iter__")();
    });

  // Drag_State — drag operation in progress.
  nb::class_<Drag_State>(m, "Drag_State")
    .def(nb::init<>())
    .def_rw("card_id", &Drag_State::card_id)
    .def_rw("current_stack", &Drag_State::current_stack)
    .def_rw("last_hovered_stack", &Drag_State::last_hovered_stack)
    .def_rw("original_stack", &Drag_State::original_stack)
    .def_rw("offset_x", &Drag_State::offset_x)
    .def_rw("offset_y", &Drag_State::offset_y);

  // Table_State — full table state passed to every render and input function.
  nb::class_<Table_State>(m, "Table_State")
    .def(nb::init<>())
    .def(
      "__init__",
      [](
        Table_State*                       t,
        nb::list                           cards,
        nb::list                           stacks,
        nb::list                           loose_cards,
        std::function<void(Table_State*)>  draw_callback,
        std::function<bool(int, int, int)> is_drop_card_allowed,
        int                                zoomed_card_id
      ) {
        new (t) Table_State();
        for (auto item : cards) t->cards.push_back(nb::cast<Card>(item));
        for (auto item : stacks) t->stacks.push_back(nb::cast<Stack>(item));
        for (auto item : loose_cards)
          t->loose_cards.push_back(nb::cast<int>(item));
        if (draw_callback) t->draw_callback = std::move(draw_callback);
        if (is_drop_card_allowed)
          t->is_drop_card_allowed = std::move(is_drop_card_allowed);
        t->zoomed_card_id = zoomed_card_id;
      },
      nb::kw_only(),
      "cards"_a                = nb::list(),
      "stacks"_a               = nb::list(),
      "loose_cards"_a          = nb::list(),
      "draw_callback"_a        = nb::none(),
      "is_drop_card_allowed"_a = nb::none(),
      "zoomed_card_id"_a       = -1
    )
    // Proxy getters store a Python reference to self so element references stay
    // valid.
    .def_prop_rw(
      "cards",
      [](nb::object self) {
        return Cards_Ref{&nb::cast<Table_State&>(self).cards, self};
      },
      [](Table_State& t, nb::list v) {
        t.cards.clear();
        for (auto item : v) t.cards.push_back(nb::cast<Card>(item));
      }
    )
    .def_prop_rw(
      "stacks",
      [](nb::object self) {
        return Stacks_Ref{&nb::cast<Table_State&>(self).stacks, self};
      },
      [](Table_State& t, nb::list v) {
        t.stacks.clear();
        for (auto item : v) t.stacks.push_back(nb::cast<Stack>(item));
      }
    )
    .def_prop_rw(
      "loose_cards",
      [](Table_State& t) -> std::vector<int>& { return t.loose_cards; },
      [](Table_State& t, nb::list v) {
        t.loose_cards.clear();
        for (auto item : v) t.loose_cards.push_back(nb::cast<int>(item));
      },
      nb::rv_policy::reference_internal
    )
    .def_rw("drag_state", &Table_State::drag_state)
    .def_prop_rw(
      "animated_cards",
      [](nb::object self) {
        return Cards_Ref{&nb::cast<Table_State&>(self).animated_cards, self};
      },
      [](Table_State& t, nb::list v) {
        t.animated_cards.clear();
        for (auto item : v) t.animated_cards.push_back(nb::cast<Card>(item));
      }
    )
    // Used by the StackVector proxy's __getitem__ to return a true Stack
    // reference.
    .def(
      "get_stack",
      [](Table_State& t, int i) -> Stack& { return t.stacks[i]; },
      nb::rv_policy::reference_internal
    )
    .def_rw("draw_callback", &Table_State::draw_callback)
    .def_rw("zoomed_card_id", &Table_State::zoomed_card_id)
    .def_rw("is_drop_card_allowed", &Table_State::is_drop_card_allowed)
    .def_rw("dropped_card", &Table_State::dropped_card)
    .def("poll_dropped_card", &Table_State::poll_dropped_card);
}
