#include <nanobind/nanobind.h>

#include "protocol.h"
#include "setup.h"

namespace nb = nanobind;

NB_MODULE(_online_cpp, m) {
    bind_protocol(m);
    bind_setup(m);
}
