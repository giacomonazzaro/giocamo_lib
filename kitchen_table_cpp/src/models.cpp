#include "kt_models.h"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/tuple.h>
#include <sstream>

namespace nb = nanobind;
using namespace nb::literals;

Table_State::Table_State()
    : animated_cards(nb::none())
    , draw_callback(nb::none())
    , is_drop_card_allowed(nb::cpp_function([](int, int, int) { return true; }))
    , dropped_card(nb::none())
{}

nb::object Table_State::poll_dropped_card() {
    nb::object result = dropped_card;
    dropped_card = nb::none();
    return result;
}

void bind_models(nb::module_& m) {
    // KT_Rectangle bound as "Rectangle" so pyray's cffi accepts it as a Rectangle argument.
    nb::class_<KT_Rectangle>(m, "Rectangle")
        .def(nb::init<>())
        .def("__init__", [](KT_Rectangle* r, float x, float y, float width, float height) {
            new (r) KT_Rectangle();
            r->x = x;
            r->y = y;
            r->width = width;
            r->height = height;
        }, "x"_a = 0.0f, "y"_a = 0.0f, "width"_a = 0.0f, "height"_a = 0.0f)
        .def_rw("x", &KT_Rectangle::x)
        .def_rw("y", &KT_Rectangle::y)
        .def_rw("width", &KT_Rectangle::width)
        .def_rw("height", &KT_Rectangle::height)
        .def("__iter__", [](const KT_Rectangle& r) {
            return nb::make_tuple(r.x, r.y, r.width, r.height).attr("__iter__")();
        })
        .def("__len__", [](const KT_Rectangle&) { return 4; })
        .def("__repr__", [](const KT_Rectangle& r) {
            std::ostringstream ss;
            ss << "Rectangle(x=" << r.x << ", y=" << r.y
               << ", width=" << r.width << ", height=" << r.height << ")";
            return ss.str();
        });

    // Thing — base visual entity with optional Python draw callback.
    nb::class_<Thing>(m, "Thing")
        .def(nb::init<>())
        .def("__init__", [](Thing* t, int id,
                             std::string image_path, float x, float y,
                             int rotation, nb::object draw_callback) {
            new (t) Thing();
            t->id = id;
            t->image_path = image_path;
            t->x = x;
            t->y = y;
            t->rotation = rotation;
            t->draw_callback = draw_callback;
        }, "id"_a = 0, nb::kw_only(),
           "image_path"_a = "",
           "x"_a = 0.0f,
           "y"_a = 0.0f,
           "rotation"_a = 0,
           "draw_callback"_a = nb::none())
        .def_rw("id", &Thing::id)
        .def_rw("image_path", &Thing::image_path)
        .def_rw("x", &Thing::x)
        .def_rw("y", &Thing::y)
        .def_rw("rotation", &Thing::rotation)
        .def_prop_rw("draw_callback",
            [](Thing& t) -> nb::object { return t.draw_callback; },
            [](Thing& t, nb::object cb) { t.draw_callback = cb; });

    // Card — inherits all Thing fields.
    nb::class_<Card, Thing>(m, "Card")
        .def(nb::init<>())
        .def("__init__", [](Card* c, int id,
                             std::string image_path, float x, float y,
                             int rotation, nb::object draw_callback) {
            new (c) Card();
            c->id = id;
            c->image_path = image_path;
            c->x = x;
            c->y = y;
            c->rotation = rotation;
            c->draw_callback = draw_callback;
        }, "id"_a = 0, nb::kw_only(),
           "image_path"_a = "",
           "x"_a = 0.0f,
           "y"_a = 0.0f,
           "rotation"_a = 0,
           "draw_callback"_a = nb::none());

    // Stack — ordered pile of cards with layout parameters.
    // First argument is a duck-typed rect; remaining args are keyword-only.
    nb::class_<Stack>(m, "Stack")
        .def("__init__", [](Stack* s, nb::object rect,
                             nb::list cards,
                             float spread_x, float spread_y,
                             bool face_up, std::string name,
                             float depth, int capacity) {
            new (s) Stack();
            s->rect.x      = nb::cast<float>(rect.attr("x"));
            s->rect.y      = nb::cast<float>(rect.attr("y"));
            s->rect.width  = nb::cast<float>(rect.attr("width"));
            s->rect.height = nb::cast<float>(rect.attr("height"));
            s->cards    = cards;
            s->spread_x = spread_x;
            s->spread_y = spread_y;
            s->face_up  = face_up;
            s->name     = name;
            s->depth    = depth;
            s->capacity = capacity;
        }, "rect"_a, nb::kw_only(),
           "cards"_a    = nb::list(),
           "spread_x"_a = 0.0f,
           "spread_y"_a = 0.0f,
           "face_up"_a  = true,
           "name"_a     = "",
           "depth"_a    = 0.0f,
           "capacity"_a = -1)
        .def_prop_rw("rect",
            [](const Stack& s) -> KT_Rectangle { return s.rect; },
            [](Stack& s, nb::object r) {
                s.rect.x      = nb::cast<float>(r.attr("x"));
                s.rect.y      = nb::cast<float>(r.attr("y"));
                s.rect.width  = nb::cast<float>(r.attr("width"));
                s.rect.height = nb::cast<float>(r.attr("height"));
            })
        .def_prop_rw("cards",
            [](Stack& s) -> nb::list { return s.cards; },
            [](Stack& s, nb::list l) { s.cards = l; })
        .def_rw("spread_x", &Stack::spread_x)
        .def_rw("spread_y", &Stack::spread_y)
        .def_rw("face_up",  &Stack::face_up)
        .def_rw("name",     &Stack::name)
        .def_rw("depth",    &Stack::depth)
        .def_rw("capacity", &Stack::capacity);

    // Drag_State — drag operation in progress.
    nb::class_<Drag_State>(m, "Drag_State")
        .def(nb::init<>())
        .def_rw("card_id",            &Drag_State::card_id)
        .def_rw("current_stack",      &Drag_State::current_stack)
        .def_rw("last_hovered_stack", &Drag_State::last_hovered_stack)
        .def_rw("original_stack",     &Drag_State::original_stack)
        .def_rw("offset_x",           &Drag_State::offset_x)
        .def_rw("offset_y",           &Drag_State::offset_y);

    // Table_State — full table state passed to every render and input function.
    nb::class_<Table_State>(m, "Table_State")
        .def(nb::init<>())
        .def_prop_rw("cards",
            [](Table_State& t) -> nb::list { return t.cards; },
            [](Table_State& t, nb::list l) { t.cards = l; })
        .def_prop_rw("stacks",
            [](Table_State& t) -> nb::list { return t.stacks; },
            [](Table_State& t, nb::list l) { t.stacks = l; })
        .def_prop_rw("loose_cards",
            [](Table_State& t) -> nb::list { return t.loose_cards; },
            [](Table_State& t, nb::list l) { t.loose_cards = l; })
        .def_rw("drag_state", &Table_State::drag_state)
        .def_prop_rw("animated_cards",
            [](Table_State& t) -> nb::object { return t.animated_cards; },
            [](Table_State& t, nb::object v) { t.animated_cards = v; })
        .def_prop_rw("draw_callback",
            [](Table_State& t) -> nb::object { return t.draw_callback; },
            [](Table_State& t, nb::object cb) { t.draw_callback = cb; })
        .def_rw("zoomed_card_id", &Table_State::zoomed_card_id)
        .def_prop_rw("is_drop_card_allowed",
            [](Table_State& t) -> nb::object { return t.is_drop_card_allowed; },
            [](Table_State& t, nb::object fn) { t.is_drop_card_allowed = fn; })
        .def_prop_rw("dropped_card",
            [](Table_State& t) -> nb::object { return t.dropped_card; },
            [](Table_State& t, nb::object v) { t.dropped_card = v; })
        .def("poll_dropped_card", &Table_State::poll_dropped_card);
}
