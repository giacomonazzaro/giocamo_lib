# Notes for C++ Conversion

This document catalogues Python patterns in the codebase that would be non-trivial to convert to idiomatic C++ with value semantics.

---

## 1. Callbacks Stored in Dataclasses

**Files:** `gods/models.py`, `kitchen_table/models.py`

The `Choice` dataclass stores two callable fields (`actions` and `resolve`). These are closures that capture game state from enclosing scopes:

```python
@dataclass(slots=True)
class Choice:
    actions: Callable[[Game_State], Choose_Card | ...]
    resolve: Callable[[Game_State, int], list[Choice]]
```

Similarly, `kt.Card` stores a `draw_callback` closure, and `Table_State` stores `is_drop_card_allowed`.

**C++ issue:** `std::function` is copyable as long as captured variables are copyable — and captured `Card*` pointers are trivially copyable. So this is fine for normal gameplay. **However**, see issue #4 for why this breaks during MCTS search.

---

## 2. Card Polymorphism via Subclasses

**File:** `gods/cards.py` (40+ card subclasses)

Cards are stored in a homogeneous `list[Card]` but dispatch to subclass-specific hook methods at runtime. 11 hook methods, 40+ card types.

**C++ approach:** Store as `std::vector<Card*>` with virtual methods — straightforward, no issue.

---

## 3. Closures Capturing Card Pointers in Card Effects

**File:** `gods/cards.py` (throughout)

Card `on_played` methods create closures that are stored in `Choice` objects and called later. See issue #4 for why capturing `self` directly was a problem for MCTS.

**Status: Fixed.** All closures now capture `my_id = self.id` (a plain `int`) and look up the card via `state.all_cards[my_id]` at call time. This is safe for both normal gameplay and MCTS cloning.

---

## 4. Deep Copy for AI Search

**Files:** `gods/agents/mcts.py:74`, `gods/agents/minimax_search.py:57,94`, `gods/agents/minimax_stochastic.py:33`

```python
sim_state = copy.deepcopy(state)
sim_choice = copy.deepcopy(choice)
```

MCTS clones the full `Game_State` — including all `Card` objects and any pending `Choice` — to simulate future moves without affecting the real game.

**C++ issue:** When deep-copying a `Game_State`, new `Card` objects are created. If closures inside the copied `Choice` captured `Card*` pointers into the original game's card array, MCTS mutations would silently corrupt the real game state. In Python, `deepcopy` rewrites all internal references automatically — C++ `std::function` copy cannot.

**Status: Fixed.** All closures now capture integer indices, so the copied `Choice` naturally uses the cloned state's cards when invoked.

---

## 5. Union Types with `isinstance` Dispatch

**File:** `gods/game.py:50-64`

```python
def action_options(actions: Choose_Card | Choose_Cards | Choose_Option | Choose_Options) -> list:
    if isinstance(actions, (Choose_Card, Choose_Option)):
        return actions.targets
    ...
```

**Pre-port refactor:** Python 3.10+ `match`/`case` structural pattern matching is the direct equivalent of `std::visit` on `std::variant` and maps cleanly to C++:

```python
match actions:
    case Choose_Card() | Choose_Option():
        return actions.targets
    case Choose_Cards() | Choose_Options():
        ...
```

Worth doing now for clarity.

---

## 6. String-Based Area and Choice Dispatch

**File:** `gods/models.py:81`, `gods/gameplay.py`, `gods_graphical/agent_ui.py`

`Card_Id.area` is a string (`"hand"`, `"wonders"`, `"deck"`, `"discard"`, `"people"`, `"none"`) used for dispatch. `Choice.description` is similarly a string (`"main"`, `"choose-card"`, `"choose-cards"`).

**Pre-port refactor:** Python's `enum.Enum` maps directly to C++ enums:

```python
from enum import Enum

class Area(Enum):
    HAND = "hand"
    WONDERS = "wonders"
    DECK = "deck"
    DISCARD = "discard"
    PEOPLE = "people"
    NONE = "none"

class Choice_Kind(Enum):
    MAIN = "main"
    CHOOSE_CARD = "choose-card"
    CHOOSE_CARDS = "choose-cards"
```

Good refactor to do now — it catches typos, enables IDE autocompletion, and maps trivially to C++ `enum class`.

---

## 7. String-to-Class Factory for Cards

**File:** `gods/cards.py`

Cards are instantiated from JSON data by looking up their class by name string. Keeping the `CARD_CLASSES` dict is fine — it maps directly to a C++ factory function (`switch`/`if-else` on an enum). No change needed before porting.

---

## 8. Optional/None Values

`Optional[int]`, `Choice | None`, `Card_Id` null sentinel — all fine. `std::optional` or sentinel values both work. No action needed.

---

## 9. Agent Polymorphism

`Agent_Duel` stores two sub-agents. **C++ approach:** `Agent*` pointers with virtual `choose_action`. Fine.

---

## 10. Draw Callbacks Capturing Game State

**File:** `gods_graphical/main.py:46-73`

Each `kt.Card` stores a `draw_power` closure that captures `gods_state`:

```python
def draw_power(card: kt.Card):
    gods_card = gods_state.all_cards[card.id]  # captures gods_state by reference
```

**C++ approach:** The closure captures a `Game_State*`. As long as `gods_state` is stable (lives on the heap or has a fixed address), this is fine — and it should be, since it's the main game state. No issue in practice if the pointer is stable.

---

## 11. Animated Cards Deep Copy in Rendering

**File:** `kitchen_table/rendering.py:378-384`

```python
if table_state.animated_cards is None:
    table_state.animated_cards = deepcopy(table_state.cards)
```

**C++ approach:** The `None` check is just a first-time initialization guard. In C++, use a `bool initialized` flag or check `animated_cards.empty()`. Since `kt.Card` contains only plain data (floats, ints, strings), a regular copy is sufficient — no deep copy semantics needed.

---

## Summary

| Issue | Status |
|-------|--------|
| Closures capturing card objects in MCTS | ✅ Fixed — all closures capture `int` index |
| Card polymorphism | No action — `std::vector<Card*>` + virtual methods |
| Agent polymorphism | No action — `Agent*` pointers |
| Callbacks in `Choice` / `kt.Card` | No action — `std::function` with pointer capture is fine |
| Draw callbacks capturing game state | No action — `Game_State*` stable at runtime |
| Optional / None values | No action — sentinel values or `std::optional` |
| Animated cards deep copy | No action — plain copy + init flag |
| `isinstance` / union type dispatch | Refactor to `match`/`case` (good for Python too) |
| String area/description dispatch | Refactor to `enum.Enum` (good for Python too) |

## Remaining Pre-Port Refactors

1. **Replace string areas with `Area` enum** — `Card_Id.area: Area`, `Choice.description: Choice_Kind`. Touches `gods/models.py`, `gods/gameplay.py`, `gods/cards.py`, `gods_graphical/agent_ui.py`.
2. **Replace `isinstance` dispatch with `match`/`case`** — in `gods/game.py` and anywhere else union types are checked.
