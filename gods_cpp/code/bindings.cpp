#ifdef GODS_BUILD_PYTHON

#include <nanobind/nanobind.h>

#include "ai.h"
#include "cards.h"
#include "gameplay.h"
#include "models.h"
#include "setup.h"

namespace nb = nanobind;

NB_MODULE(_gods_cpp, m) {
  bind_models(m);
  bind_gameplay(m);
  bind_cards(m);
  bind_setup(m);
  bind_agent(m);
}

#endif // GODS_BUILD_PYTHON
