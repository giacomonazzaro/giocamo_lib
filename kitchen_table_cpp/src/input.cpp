#include "kt_input.h"
#include "kt_game_state.h"
#include "kt_config.h"
#include "raylib.h"
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <algorithm>
#include <random>
#include <cstdio>
namespace nb = nanobind;
using namespace nb::literals;


bool stack_is_full(const Stack& stack) {
    // Returns true if the stack has a capacity limit and has reached it.
    return stack.capacity >= 0 && (int)nb::len(stack.cards) >= stack.capacity;
}


bool point_in_card(float px, float py, const Thing& card) {
    // Check if point (px, py) is inside the card bounds.
    float w = (float)kt::CARD_WIDTH;
    float h = (float)kt::CARD_HEIGHT;
    return (card.x <= px && px <= card.x + w && card.y <= py && py <= card.y + h);
}


bool card_pressed(const Thing& card) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return false;
    float mx = (float)GetMouseX();
    float my = (float)GetMouseY();
    return point_in_card(mx, my, card);
}


bool point_in_stack_area(float px, float py, const Stack& stack) {
    // Check if point is in the stack's general area (for drop targets).
    float h = (float)kt::CARD_HEIGHT;
    float x = stack.rect.x;
    float y = stack.rect.y;
    float w = stack.rect.width;
    return (x <= px && px <= x + w && y <= py && py <= y + h);
}


nb::object find_card_at(float px, float py, Table_State& state) {
    // Find the topmost card at position. Returns (card_id, stack_index) or None.
    // For loose cards, stack_index is -1.

    // Check loose cards first (on top of everything).
    size_t loose_len = (size_t)nb::len(state.loose_cards);
    for (size_t i = loose_len; i-- > 0; ) {
        int card_id = nb::cast<int>(state.loose_cards[(size_t)i]);
        Thing& card = nb::cast<Thing&>(state.cards[(size_t)card_id]);
        if (point_in_card(px, py, card))
            return nb::make_tuple(card_id, -1);
    }

    // Check stacks (reverse order so we check top cards first).
    size_t stacks_len = (size_t)nb::len(state.stacks);
    for (size_t stack_idx = stacks_len; stack_idx-- > 0; ) {
        Stack& stack = nb::cast<Stack&>(state.stacks[(size_t)stack_idx]);
        size_t cards_len = (size_t)nb::len(stack.cards);
        for (size_t j = cards_len; j-- > 0; ) {
            int card_id = nb::cast<int>(stack.cards[(size_t)j]);
            Thing& card = nb::cast<Thing&>(state.cards[(size_t)card_id]);
            if (point_in_card(px, py, card))
                return nb::make_tuple(card_id, (int)stack_idx);
        }
    }
    return nb::none();
}


int find_stack_at(float px, float py, const Table_State& state) {
    // Find a stack at the given position. Returns stack index or -1.
    size_t stacks_len = (size_t)nb::len(state.stacks);
    for (size_t i = 0; i < stacks_len; i++) {
        const Stack& stack = nb::cast<const Stack&>(state.stacks[(size_t)i]);
        if (point_in_stack_area(px, py, stack))
            return (int)i;
    }
    return -1;
}


void handle_mouse_press(Table_State& state) {
    // Handle mouse button press — start drag.
    float mx = (float)GetMouseX();
    float my = (float)GetMouseY();
    Drag_State& drag = state.drag_state;

    // Check if clicking a card.
    nb::object result = find_card_at(mx, my, state);
    if (result.is_none())
        return;

    nb::tuple result_tup = nb::cast<nb::tuple>(result);
    int card_id   = nb::cast<int>(result_tup[0]);
    int stack_idx = nb::cast<int>(result_tup[1]);
    Thing& card = nb::cast<Thing&>(state.cards[(size_t)card_id]);

    drag.card_id        = card_id;
    drag.current_stack  = stack_idx;
    drag.original_stack = stack_idx;
    drag.offset_x       = mx - card.x;
    drag.offset_y       = my - card.y;
}


void handle_mouse_release(Table_State& state) {
    // Handle mouse button release — drop card.
    Drag_State& drag = state.drag_state;
    if (drag.card_id < 0)
        return;

    bool allowed = nb::cast<bool>(state.is_drop_card_allowed(
        nb::int_(drag.original_stack), nb::int_(drag.current_stack), nb::int_(drag.card_id)));

    if (!allowed) {
        if (drag.current_stack == -1) {
            Stack& orig = nb::cast<Stack&>(state.stacks[(size_t)drag.original_stack]);
            orig.cards.append(nb::int_(drag.card_id));
        }
    }

    // Signal the drop event as (from_stack, to_stack, card_id) — matches Python original.
    state.dropped_card = nb::make_tuple(drag.original_stack, drag.current_stack, drag.card_id);

    int original_stack = drag.original_stack;
    int current_stack  = drag.current_stack;
    state.drag_state   = Drag_State();

    Stack& orig_stack = nb::cast<Stack&>(state.stacks[(size_t)original_stack]);
    update_card_positions(orig_stack, state, /*sort=*/true);

    if (current_stack >= 0) {
        Stack& cur_stack = nb::cast<Stack&>(state.stacks[(size_t)current_stack]);
        update_card_positions(cur_stack, state, /*sort=*/true);
    }
}


void handle_mouse_move(Table_State& state) {
    // Update dragged card position.
    Drag_State& drag = state.drag_state;
    if (drag.card_id < 0)
        return;

    float mx = (float)GetMouseX();
    float my = (float)GetMouseY();
    Thing& card = nb::cast<Thing&>(state.cards[(size_t)drag.card_id]);
    card.x = mx - drag.offset_x;
    card.y = my - drag.offset_y;

    int hovered_stack = find_stack_at(mx, my, state);
    if (hovered_stack < 0)
        return;

    if (drag.last_hovered_stack != hovered_stack) {
        // Remove card from current stack if it's there.
        if (drag.current_stack >= 0) {
            Stack& cur = nb::cast<Stack&>(state.stacks[(size_t)drag.current_stack]);
            bool has = nb::cast<bool>(cur.cards.attr("__contains__")(nb::int_(drag.card_id)));
            if (has) {
                cur.cards.attr("remove")(nb::int_(drag.card_id));
                update_card_positions(cur, state, /*sort=*/true);
            }
        }

        nb::object hovered_obj = state.stacks[(size_t)hovered_stack];
        Stack& hovered = nb::cast<Stack&>(hovered_obj);
        bool allowed = nb::cast<bool>(state.is_drop_card_allowed(
            nb::int_(drag.original_stack), nb::int_(hovered_stack), nb::int_(drag.card_id)));
        bool full = stack_is_full(hovered);
        fprintf(stderr, "[drag] orig=%d hover=%d card=%d allowed=%d full=%d\n",
                drag.original_stack, hovered_stack, drag.card_id, (int)allowed, (int)full);

        if (allowed && !full) {
            hovered.cards.append(nb::int_(drag.card_id));
            update_card_positions(hovered, state, /*sort=*/true);
            drag.current_stack = hovered_stack;
        } else {
            drag.current_stack = -1;
        }
    }

    Stack& hovered = nb::cast<Stack&>(state.stacks[(size_t)hovered_stack]);
    update_card_positions(hovered, state, /*sort=*/true);
    drag.last_hovered_stack = hovered_stack;
}


void handle_rotate_card(Table_State& state, bool clockwise) {
    // Rotate the card under the cursor by 90 degrees.
    float mx = (float)GetMouseX();
    float my = (float)GetMouseY();

    nb::object result = find_card_at(mx, my, state);
    if (result.is_none())
        return;

    int card_id = nb::cast<int>(nb::cast<nb::tuple>(result)[0]);
    Thing& card = nb::cast<Thing&>(state.cards[(size_t)card_id]);
    if (clockwise)
        card.rotation = card.rotation + 90;
    else
        card.rotation = card.rotation - 90;
}


void shuffle_stack(Table_State& state, int stack_id) {
    if (stack_id == -1)
        return;

    Stack& stack = nb::cast<Stack&>(state.stacks[(size_t)stack_id]);

    // Collect card ids into a std::vector, shuffle, rebuild the list.
    size_t n = (size_t)nb::len(stack.cards);
    std::vector<int> ids(n);
    for (size_t i = 0; i < n; i++)
        ids[i] = nb::cast<int>(stack.cards[(size_t)i]);

    static std::mt19937 rng{ std::random_device{}() };
    std::shuffle(ids.begin(), ids.end(), rng);

    nb::list new_cards;
    for (int id : ids) new_cards.append(nb::int_(id));
    stack.cards = new_cards;

    update_card_positions(stack, state, /*sort=*/false);
}


void update_input(Table_State& state) {
    // Main input processing — call each frame.
    state.dropped_card = nb::none();

    // Handle mouse.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        handle_mouse_press(state);
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        handle_mouse_release(state);
    }

    // Update drag position continuously.
    handle_mouse_move(state);

    // Handle card rotation.
    if (IsKeyPressed(KEY_R)) {
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        handle_rotate_card(state, /*clockwise=*/!shift);
    }

    // Handle card zoom.
    float mx = (float)GetMouseX();
    float my = (float)GetMouseY();

    if (IsKeyDown(KEY_SPACE)) {
        nb::object result = find_card_at(mx, my, state);
        if (!result.is_none())
            state.zoomed_card_id = nb::cast<int>(nb::cast<nb::tuple>(result)[0]);
        else
            state.zoomed_card_id = -1;
    } else {
        state.zoomed_card_id = -1;
    }

    if (IsKeyPressed(KEY_S)) {
        int stack_id = find_stack_at(mx, my, state);
        shuffle_stack(state, stack_id);
    }
}


void bind_input(nb::module_& m) {
    m.def("stack_is_full",        &stack_is_full,        "stack"_a);
    m.def("point_in_card",        &point_in_card,        "px"_a, "py"_a, "card"_a);
    m.def("card_pressed",         &card_pressed,         "card"_a);
    m.def("point_in_stack_area",  &point_in_stack_area,  "px"_a, "py"_a, "stack"_a);
    m.def("find_card_at",         &find_card_at,         "px"_a, "py"_a, "state"_a);
    m.def("find_stack_at",        &find_stack_at,        "px"_a, "py"_a, "state"_a);
    m.def("handle_mouse_press",   &handle_mouse_press,   "state"_a);
    m.def("handle_mouse_release", &handle_mouse_release, "state"_a);
    m.def("handle_mouse_move",    &handle_mouse_move,    "state"_a);
    m.def("handle_rotate_card",   &handle_rotate_card,   "state"_a, "clockwise"_a = true);
    m.def("shuffle_stack",        &shuffle_stack,        "state"_a, "stack_id"_a);
    m.def("update_input",         &update_input,         "state"_a);
}
