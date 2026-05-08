#include <nanobind/nanobind.h>
#include "kt_models.h"
#include "kt_config.h"
#include "kt_game_state.h"
#include "kt_input.h"
#include "kt_rendering.h"

namespace nb = nanobind;

NB_MODULE(_kt_cpp, m) {
    bind_models(m);
    bind_config(m);
    bind_game_state(m);
    bind_input(m);
    bind_rendering(m);
}
