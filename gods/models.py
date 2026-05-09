"""Python shim for the C++ gods models — re-exports symbols from gods._gods_cpp.
The real implementation lives in gods_cpp/. This module exists so existing
code can keep importing `from gods.models import ...` unchanged."""

from gods._gods_cpp import (
    Card,
    Card_Color,
    Card_Design,
    Card_Id,
    Card_Type,
    Game_State,
    Player,
    get_card_designs as _get_card_designs,
)


class _Card_Designs_Proxy:
    """Live view into the C++ card_designs registry.

    Re-fetches from C++ on every access so changes made via
    `gods._gods_cpp.set_card_designs(...)` are visible immediately.
    The clear/extend methods are no-ops kept for backwards compatibility
    with code that mutated the old Python list in place; setup paths now
    call `set_card_designs` instead."""

    def __getitem__(self, i):
        return _get_card_designs()[i]

    def __len__(self):
        return len(_get_card_designs())

    def __iter__(self):
        return iter(_get_card_designs())

    def clear(self):
        pass

    def extend(self, _):
        pass


card_designs = _Card_Designs_Proxy()
