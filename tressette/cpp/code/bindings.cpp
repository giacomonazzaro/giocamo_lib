#include <nanobind/nanobind.h>

#include "ai.h"
#include "gameplay.h"
#include "models.h"

namespace nb = nanobind;

NB_MODULE(_tressette_cpp, m) {
  bind_models(m);
  bind_gameplay(m);
  bind_agent(m);
}
