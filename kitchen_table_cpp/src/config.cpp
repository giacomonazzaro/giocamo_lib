#include "kt_config.h"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
namespace nb = nanobind;

void bind_config(nb::module_& m) {
    nb::dict d;

    // Window settings.
    d["window_width"]  = nb::int_(kt::WINDOW_WIDTH);
    d["window_height"] = nb::int_(kt::WINDOW_HEIGHT);
    d["window_title"]  = nb::str(kt::WINDOW_TITLE);
    d["target_fps"]    = nb::int_(kt::TARGET_FPS);

    // Card dimensions.
    d["card_width"]         = nb::int_(kt::CARD_WIDTH);
    d["card_height"]        = nb::int_(kt::CARD_HEIGHT);
    d["card_corner_radius"] = nb::int_(kt::CARD_CORNER_RADIUS);
    d["card_padding"]       = nb::int_(kt::CARD_PADDING);

    // Card colors.
    d["card_background"]      = nb::make_tuple(255, 255, 255, 255);
    d["card_border"]          = nb::make_tuple(80,  80,  80,  255);
    d["card_back"]            = nb::make_tuple(60,  80,  120, 255);
    d["card_back_pattern"]    = nb::make_tuple(80,  100, 140, 255);
    d["card_title_color"]     = nb::make_tuple(30,  30,  30,  255);
    d["card_description_color"] = nb::make_tuple(80, 80,  80,  255);

    // Font settings.
    d["title_font_size"]       = nb::int_(kt::TITLE_FONT_SIZE);
    d["description_font_size"] = nb::int_(kt::DESC_FONT_SIZE);
    d["power_font_size"]       = nb::int_(kt::POWER_FONT_SIZE);
    d["font_path"]             = nb::str(kt::FONT_PATH);
    d["font_spacing"]          = nb::float_(kt::FONT_SPACING);
    d["font_load_size"]        = nb::int_(kt::FONT_LOAD_SIZE);

    // Stack spread (how much cards offset from each other).
    d["hand_spread_x"]    = nb::int_(kt::HAND_SPREAD_X);
    d["pile_spread_y"]    = nb::int_(kt::PILE_SPREAD_Y);
    d["wonders_spread_x"] = nb::int_(kt::WONDERS_SPREAD_X);

    // UI colors.
    d["button_color"]          = nb::make_tuple(70,  130, 180, 255);
    d["button_hover_color"]    = nb::make_tuple(90,  150, 200, 255);
    d["button_text_color"]     = nb::make_tuple(255, 255, 255, 255);
    d["highlight_color"]       = nb::make_tuple(255, 215, 0,   200);
    d["current_player_color"]  = nb::make_tuple(100, 200, 100, 255);
    d["modal_overlay"]         = nb::make_tuple(0,   0,   0,   180);

    m.attr("tweak") = d;
}
