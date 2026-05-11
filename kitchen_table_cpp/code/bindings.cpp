#include <nanobind/nanobind.h>

#include "config.h"
#include "game_state.h"
#include "input.h"
#include "models.h"
#include "rendering.h"
#include "ui.h"

namespace nb = nanobind;

NB_MODULE(_kt_cpp, m) {
  bind_models(m);
  bind_config(m);
  bind_game_state(m);
  bind_input(m);
  bind_rendering(m);
  bind_ui(m);
}
