#include <nanobind/nanobind.h>

#include "ai.h"
#include "gameplay.h"
#include "models.h"

namespace nb = nanobind;

NB_MODULE(_tressette_cpp, m) {
  // Pull in Game / Choice / Agent / game_frame from the shared game_cpp
  // bindings so the tressette types can extend them and Python users see one
  // consistent class hierarchy.
  nb::module_::import_("game._game_cpp");

  bind_models(m);
  bind_gameplay(m);
  bind_agent(m);
}
