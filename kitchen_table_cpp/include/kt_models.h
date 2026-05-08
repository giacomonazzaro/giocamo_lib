#pragma once
#include <string>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

namespace nb = nanobind;

// 2D rectangle matching pyray's Rectangle layout (x, y, width, height).
// Implements __iter__ and __len__ so pyray's cffi accepts it as a Rectangle argument.
struct KT_Rectangle {
    float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
};

// Base visual entity with optional Python draw callback.
struct Thing {
    int id = 0;
    std::string image_path;
    float x = 0.0f, y = 0.0f;
    int rotation = 0;
    nb::object draw_callback;  // Python callable or None.

    Thing() : draw_callback(nb::none()) {}
};

// A visual card — inherits all Thing fields.
struct Card : Thing {
    using Thing::Thing;
};

// An ordered pile of cards with layout parameters.
struct Stack {
    KT_Rectangle rect;
    nb::list cards;        // Python list of int card IDs.
    float spread_x = 0.0f, spread_y = 0.0f;
    bool face_up = true;
    std::string name;
    float depth = 0.0f;
    int capacity = -1;    // -1 = unlimited.
};

// Drag operation in progress.
struct Drag_State {
    int card_id = -1;
    int current_stack = -1;
    int last_hovered_stack = -1;
    int original_stack = -1;
    float offset_x = 0.0f, offset_y = 0.0f;
};

// Full table state passed to every render and input function.
struct Table_State {
    nb::list cards;         // list[Thing]
    nb::list stacks;        // list[Stack]
    nb::list loose_cards;   // list[int]
    Drag_State drag_state;
    nb::object animated_cards;          // None until first draw_table(); then list[Card].
    nb::object draw_callback;           // Python callable(table_state) or None.
    int zoomed_card_id = -1;
    nb::object is_drop_card_allowed;    // Python callable(src, tgt, card_id) -> bool.
    nb::object dropped_card;            // None or tuple(src, tgt, card_id) after a drop.

    Table_State();
    // Returns dropped_card and resets it to None (consume-once event poll).
    nb::object poll_dropped_card();
};

void bind_models(nb::module_& m);
