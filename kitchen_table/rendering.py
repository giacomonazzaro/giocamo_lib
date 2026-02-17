from __future__ import annotations
import os
from pyray import *
import raylib as rl
from kitchen_table.models import Card, Stack, Table_State
from kitchen_table.config import tweak

# Background shader
_background_shader = None
_background_time_loc = -1
_background_resolution_loc = -1


def _load_background_shader():
    global _background_shader, _background_time_loc, _background_resolution_loc
    shader_path = os.path.join(os.path.dirname(__file__), "background.fs")
    with open(shader_path) as f:
        fs_code = f.read()
    _background_shader = load_shader_from_memory(rl.ffi.NULL, fs_code.encode())
    _background_time_loc = get_shader_location(_background_shader, "u_time")
    _background_resolution_loc = get_shader_location(_background_shader, "u_resolution")


def draw_background():
    global _background_shader
    if _background_shader is None:
        _load_background_shader()

    t = rl.ffi.new("float *", get_time())
    rl.SetShaderValue(_background_shader, _background_time_loc, t,
                      rl.SHADER_UNIFORM_FLOAT)

    res = rl.ffi.new("float[2]", [get_screen_width(), get_screen_height()])
    rl.SetShaderValue(_background_shader, _background_resolution_loc, res,
                      rl.SHADER_UNIFORM_VEC2)

    begin_shader_mode(_background_shader)
    draw_rectangle(0, 0, get_screen_width(), get_screen_height(), WHITE)
    end_shader_mode()


# Texture cache to avoid reloading images
_texture_cache: dict[str, Texture2D] = {}


def get_texture(image_path: str) -> Texture2D | None:
    """Load and cache a texture from disk."""
    if image_path in _texture_cache:
        return _texture_cache[image_path]

    if not file_exists(image_path.encode()):
        return None

    texture = load_texture(image_path.encode())
    _texture_cache[image_path] = texture
    return texture


_rounded_texture_cache: dict[str, Texture2D] = {}


def get_rounded_texture(image_path: str) -> Texture2D | None:
    """Load a texture with rounded corners applied, using cache."""
    if image_path in _rounded_texture_cache:
        return _rounded_texture_cache[image_path]

    if not file_exists(image_path.encode()):
        return None

    w = tweak["card_width"]
    h = tweak["card_height"]
    r = tweak["card_corner_radius"]

    # Load image at original resolution (GPU scales when drawing)
    image = load_image(image_path.encode())
    iw = image.width
    ih = image.height

    # Scale corner radius to match image resolution
    sr = int(r * min(iw / w, ih / h))

    # Create rounded rectangle mask at image resolution
    mask = gen_image_color(iw, ih, Color(0, 0, 0, 0))
    image_draw_rectangle(mask, sr, 0, iw - 2 * sr, ih, WHITE)
    image_draw_rectangle(mask, 0, sr, iw, ih - 2 * sr, WHITE)
    image_draw_circle(mask, sr, sr, sr, WHITE)
    image_draw_circle(mask, iw - sr, sr, sr, WHITE)
    image_draw_circle(mask, sr, ih - sr, sr, WHITE)
    image_draw_circle(mask, iw - sr, ih - sr, sr, WHITE)

    # Apply mask and convert to GPU texture
    image_alpha_mask(image, mask)
    texture = load_texture_from_image(image)

    unload_image(image)
    unload_image(mask)

    _rounded_texture_cache[image_path] = texture
    return texture


def color_from_tuple(c: tuple) -> Color:
    """Convert RGBA tuple to raylib Color."""
    return Color(c[0], c[1], c[2], c[3])


def draw_card_back() -> None:
    x = 0
    y = 0
    """Draw a face-down card."""
    w = tweak["card_width"]
    h = tweak["card_height"]
    r = tweak["card_corner_radius"]

    # Card background
    draw_rectangle_rounded(
        Rectangle(x, y, w, h), r / min(w, h), 8,
        color_from_tuple(tweak["card_back"])
    )

    # Simple pattern on back
    pattern_color = color_from_tuple(tweak["card_back_pattern"])
    margin = 15
    draw_rectangle_rounded(
        Rectangle(x + margin, y + margin, w - 2*margin, h - 2*margin),
        r / min(w, h), 8, pattern_color
    )

    # Border
    draw_rectangle_rounded_lines_ex(
        Rectangle(x, y, w, h), r / min(w, h), 8, 2,
        color_from_tuple(tweak["card_border"])
    )


def draw_card_content(card: Card, face_up: bool) -> None:
    """Draw card content at origin (used for rotation)."""
    if not face_up:
        draw_card_back()
        return

    x = 0
    y = 0
    w = tweak["card_width"]
    h = tweak["card_height"]
    r = tweak["card_corner_radius"]
    padding = tweak["card_padding"]

    # Card background (image or solid color)
    texture = None
    if card.image_path:
        texture = get_rounded_texture(card.image_path)

    if texture:
        # Draw image scaled to fit card
        source_rect = Rectangle(0, 0, texture.width, texture.height)
        dest_rect = Rectangle(x, y, w, h)
        draw_texture_pro(texture, source_rect, dest_rect, Vector2(0, 0), 0, WHITE)
    else:
        # Fallback to solid color background
        draw_rectangle_rounded(
            Rectangle(x, y, w, h), r / min(w, h), 8,
            color_from_tuple(tweak["card_background"])
        )

    if card.draw_callback:
        card.draw_callback(card)

    return
    # Border
    draw_rectangle_rounded_lines_ex(
        Rectangle(x, y, w, h), r / min(w, h), 8, 2,
        color_from_tuple(tweak["card_border"])
    )

    # Title with background for readability when image is present
    title_size = tweak["title_font_size"]
    title_text = card.title
    title_width = measure_text(title_text, title_size)
    if texture:
        # Draw semi-transparent background behind title
        draw_rectangle(
            int(x + padding - 2),
            int(y + padding - 2),
            title_width + 4,
            title_size + 4,
            Color(0, 0, 0, 180)
        )
    draw_text(
        title_text,
        int(x + padding),
        int(y + padding),
        title_size,
        color_from_tuple(tweak["card_title_color"])
    )

    # Description (with simple word wrapping)
    if card.description:
        desc_size = tweak["description_font_size"]
        desc_y = y + padding + title_size + 10
        max_width = w - 2 * padding

        # Simple word wrapping
        words = card.description.split()
        lines = []
        current_line = ""
        for word in words:
            test_line = current_line + " " + word if current_line else word
            text_width = measure_text(test_line, desc_size)
            if text_width <= max_width:
                current_line = test_line
            else:
                if current_line:
                    lines.append(current_line)
                current_line = word
        if current_line:
            lines.append(current_line)

        # Draw background for description if image is present
        if texture and lines:
            total_height = len(lines) * (desc_size + 2)
            draw_rectangle(
                int(x + padding - 2),
                int(desc_y - 2),
                int(max_width + 4),
                int(total_height + 4),
                Color(0, 0, 0, 180)
            )

        for i, line in enumerate(lines):
            draw_text(
                line,
                int(x + padding),
                int(desc_y + i * (desc_size + 2)),
                desc_size,
                color_from_tuple(tweak["card_description_color"])
            )


def draw_card(card: Card, face_up: bool = True) -> None:
    """Draw a single card at its position, with rotation support."""
    w = tweak["card_width"]
    h = tweak["card_height"]

    # Apply rotation around center
    # Calculate center of card
    if card.rotation == 0:
        cx = card.x
        cy = card.y
        rl_push_matrix()
        rl_translatef(cx, cy, 0)
        draw_card_content(card, face_up)
        rl_pop_matrix()
    else:
        # Calculate center of card
        cx = card.x + w / 2
        cy = card.y + h / 2
        # Apply rotation around center
        rl_push_matrix()
        rl_translatef(cx, cy, 0)
        rl_rotatef(card.rotation, 0, 0, 1)
        rl_translatef(-w/2, -h/2, 0)
        draw_card_content(card, face_up)
        rl_pop_matrix()


def draw_stack(stack: Stack, state: Table_State) -> None:
    """Draw all cards in a stack."""
    for card_id in stack.cards:
        card = state.cards[card_id]
        draw_card(card, face_up=stack.face_up)


def draw_stack_placeholder(stack: Stack) -> None:
    """Draw an empty stack placeholder with label."""
    w = tweak["card_width"]
    h = tweak["card_height"]
    r = tweak["card_corner_radius"]

    # Dashed outline placeholder
    draw_rectangle_rounded_lines_ex(
        Rectangle(stack.x, stack.y, stack.width, h), r / min(w, h), 8, 1,
        Color(100, 100, 100, 100)
    )

    # Label
    label = stack.name
    text_width = measure_text(label, 14)
    draw_text(
        label,
        int(stack.x + (w - text_width) / 2),
        int(stack.y + h / 2 - 7),
        14,
        Color(100, 100, 100, 150)
    )


def animate(cards, state, dt: float = 0.1) -> None:
    # Interpolate card positions
    for r_card, t_card in zip(cards, state.cards):
        old_pos_x = r_card.x
        r_card.x = r_card.x * (1 - dt) + t_card.x * dt
        r_card.y = r_card.y * (1 - dt) + t_card.y * dt
        v_x = r_card.x - old_pos_x
        r_card.rotation = r_card.rotation * (1 - dt) + t_card.rotation * dt
        r_card.rotation += v_x * 0.1

    selected_card_id = state.drag_state.card_id
    if selected_card_id >= 0:
        cards[selected_card_id].x = state.cards[selected_card_id].x
        cards[selected_card_id].y = state.cards[selected_card_id].y

def draw_zoomed_card(card: Card) -> None:
    """Draw a card fullscreen as a zoom preview."""
    screen_w = get_screen_width()
    screen_h = get_screen_height()

    # Dim background
    draw_rectangle(0, 0, screen_w, screen_h, Color(0, 0, 0, 160))

    # Scale card to fit screen with some margin
    card_w = tweak["card_width"]
    card_h = tweak["card_height"]
    margin = 40
    scale = min((screen_w - 2 * margin) / card_w, (screen_h - 2 * margin) / card_h)

    # Center on screen
    cx = (screen_w - card_w * scale) / 2
    cy = (screen_h - card_h * scale) / 2

    rl_push_matrix()
    rl_translatef(cx, cy, 0)
    rl_scalef(scale, scale, 1)
    draw_card_content(card, face_up=True)
    rl_pop_matrix()


from copy import deepcopy

def draw_table(table_state: Table_State) -> None:
    if table_state.animated_cards is None:
        table_state.animated_cards = deepcopy(table_state.cards)

    animate(table_state.animated_cards, table_state)

    drag = table_state.drag_state

    # Draw stack placeholders for empty stacks
    if drag.source_stack != -1:
        stack = table_state.stacks[drag.source_stack]
        draw_stack_placeholder(stack)

    # Draw stacks,
    for stack in table_state.stacks:
        for card_id in stack.cards:
            card = table_state.animated_cards[card_id]
            draw_card(card, face_up=stack.face_up)

    # Draw loose cards
    for card_id in table_state.loose_cards:
        card = table_state.animated_cards[card_id]
        draw_card(card, face_up=True)

    if table_state.draw_callback is not None:
        table_state.draw_callback(table_state)

    # Draw zoomed card on top of everything
    if table_state.zoomed_card_id >= 0:
        draw_zoomed_card(table_state.cards[table_state.zoomed_card_id])