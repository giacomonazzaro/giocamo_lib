# tabletop C++ Conversion Progress

## Goal
Convert `tabletop/` Python module to C++ with nanobind bindings so
`gods_graphical/` Python code imports it unchanged.

## Status (resume point)

### Done
- [x] nanobind installed in `venv/`
- [x] `tabletop_cpp/CMakeLists.txt` — downloads raylib 5.5 headers via `file(DOWNLOAD)`,
      links with `-undefined dynamic_lookup` so pyray's _raylib_cffi.so provides symbols at runtime
- [x] All 5 header files in `tabletop_cpp/include/`
- [x] Stub `.cpp` files in `tabletop_cpp/src/` (compile but do nothing)

### Remaining
- [ ] `src/models.cpp` — nanobind bindings for Thing, Card, Stack, Drag_State, Table_State
- [ ] `src/config.cpp` — expose `tweak` dict to Python
- [ ] `src/game_state.cpp` — layout functions (no Raylib, pure math)
- [ ] `src/input.cpp` — Raylib input handling (drag, zoom, rotate, shuffle)
- [ ] `src/rendering.cpp` — Raylib drawing (shaders, textures, fonts, animation)
- [ ] Python shim files (replace each `tabletop/*.py` with 1-liner re-exports from `_kt_cpp`)
- [ ] Build test: `cmake -S tabletop_cpp -B tabletop_cpp/build -DPython_EXECUTABLE=venv/bin/python`
- [ ] Integration test: `sh run.sh`

## Key Design Decisions

### Rectangle interop
`KT_Rectangle` is bound as `tabletop._kt_cpp.Rectangle`. Implements `__iter__` yielding
`(x, y, width, height)` and `__len__` = 4, so pyray's cffi accepts it as a Rectangle parameter.
Stack constructor reads `.x .y .width .height` via `nb::object` duck-typing.

### Python callbacks
`draw_callback` (on Thing/Table_State) and `is_drop_card_allowed` (Table_State) stored as
`nb::object`. Called from C++ with `callback(nb::cast(arg))`.

### Mutable lists
`Stack.cards`, `Table_State.cards`, etc. stored as `nb::list` — literally Python list objects.
C++ iterates them with `nb::cast<int>(item)`.

### Raylib symbols
`_kt_cpp.so` compiled with raylib headers but linked `-undefined dynamic_lookup`.
All Raylib symbols provided at runtime by pyray's `_raylib_cffi.so`.

### `ui.py` stays Python
`ui.py` (Button, UI_State, place_next, place_inside, immediate_button) stays pure Python.
It imports `render_text`, `text_width`, `color_from_tuple` from the shim.

### Color interop
`color_from_tuple(tuple) -> pyray.Color` — C++ constructs it via
`nb::module_::import_("pyray").attr("Color")(r, g, b, a)`.
`render_text(..., nb::object color)` — C++ extracts `.r .g .b .a` from the cffi cdata.

## File Map

```
tabletop_cpp/
  CMakeLists.txt
  include/
    kt_models.h       — KT_Rectangle, Thing, Card, Stack, Drag_State, Table_State
    kt_config.h       — C++ constants + bind_config()
    kt_game_state.h   — layout function declarations
    kt_input.h        — input function declarations
    kt_rendering.h    — rendering function declarations
  src/
    models.cpp        — struct impls + nanobind class bindings
    config.cpp        — tweak dict exposed to Python
    game_state.cpp    — layout logic
    input.cpp         — Raylib input handling
    rendering.cpp     — Raylib draw calls
    bindings.cpp      — NB_MODULE(_kt_cpp) calling all bind_* functions

tabletop/        — becomes shim package after conversion
  __init__.py         — UNCHANGED
  ui.py               — UNCHANGED
  background.fs       — UNCHANGED
  models.py           — → from tabletop._kt_cpp import Thing, Card, ...
  config.py           — → from tabletop._kt_cpp import tweak
  game_state.py       — → from tabletop._kt_cpp import ...
  input.py            — → from tabletop._kt_cpp import ...
  rendering.py        — → from tabletop._kt_cpp import ...
```

## Build Command
```bash
cd /path/to/gods-app
cmake -S tabletop_cpp -B tabletop_cpp/build \
  -DPython_EXECUTABLE=venv/bin/python \
  -DCMAKE_BUILD_TYPE=Release
cmake --build tabletop_cpp/build --parallel 4
cmake --install tabletop_cpp/build
# Installs _kt_cpp.cpython-312-darwin.so into tabletop/
```

## Python Source Files (reference for porting)
- `tabletop/models.py`      — Thing, Card, Stack, Drag_State, Table_State
- `tabletop/config.py`      — tweak dict
- `tabletop/game_state.py`  — layout functions
- `tabletop/input.py`       — input functions
- `tabletop/rendering.py`   — rendering functions
