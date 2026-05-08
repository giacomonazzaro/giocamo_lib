# Gods card game package
# pyray must be imported before _kt_cpp.so loads — the C++ extension resolves
# Raylib symbols from pyray's already-loaded libraylib at import time (flat namespace).
import pyray  # noqa: F401
