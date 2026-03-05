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

Card `on_played` methods create closures that are stored in `Choice` objects and called later:

```python
def on_played(self, game: Game_State) -> list[Choice]:
    miracle_card = self  # captures the card object
    def resolve(state: Game_State, option_index: int) -> list[Choice]:
        card.counters += effective_power(state, miracle_card)
    return [Choice(..., resolve=resolve)]
```

**C++ approach:** Since cards are stored in `std::vector<Card*>` that is never modified at runtime, capturing `Card*` in a lambda is safe for normal gameplay. No pointer invalidation. See issue #4 for the MCTS problem.

---

## 4. Deep Copy for AI Search (Critical)

**Files:** `gods/agents/mcts.py:74`, `gods/agents/minimax_search.py:57,94`, `gods/agents/minimax_stochastic.py:33`

```python
sim_state = copy.deepcopy(state)
sim_choice = copy.deepcopy(choice)
```

MCTS clones the full `Game_State` — including all `Card` objects and any pending `Choice` — to simulate future moves without affecting the real game.

**C++ issue:** This is the one real problem. When you deep-copy a `Game_State`, you create new `Card` objects. But the closures inside the copied `Choice` still capture `Card*` pointers into the **original** game's card array. So when the MCTS simulation mutates `card.counters`, it silently corrupts the original game state. In Python, `deepcopy` handles this correctly by rewriting all internal references to point into the new copy — C++ `std::function` copy cannot do this.

**Fix:** Closures must capture card indices (ints), not pointers. Then they look up the card in the `state` parameter that is always passed explicitly:

```python
# Before: captures card object directly
miracle_card = self
def resolve(state, idx):
    effective_power(state, miracle_card)  # points to original, not clone

# After: captures index, looks up in the state passed at call time
miracle_card_index = self.id  # card_index, a plain int
def resolve(state, idx):
    miracle_card = state.all_cards[miracle_card_index]  # always uses the right state
    effective_power(state, miracle_card)
```

This pattern is already used correctly in several cards — but inconsistently. Making it universal is the key pre-port refactor.

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

**File:** `gods/cards.py:595-636`

```python
CARD_CLASSES: dict[str, type] = {"Light": Light, "Moon": Moon, ...}

def _get_card_class(name: str) -> type:
    return CARD_CLASSES.get(name, Card)
```

**Pre-port refactor:** Replace with explicit `match`/`case` in Python, which maps to a C++ factory function. No semantic difference, but explicit and IDE-navigable:

```python
def make_card(name: str, **kwargs) -> Card:
    match name:
        case "Light":   return Light(**kwargs)
        case "Moon":    return Moon(**kwargs)
        ...
        case _:         return Card(**kwargs)
```

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

| Issue | Action needed |
|-------|--------------|
| Closures capturing `Card*` vs clone in MCTS | **Fix before porting** — capture indices, not pointers |
| `isinstance` / union type dispatch | Refactor to `match`/`case` (good for Python too) |
| String area/description dispatch | Refactor to `enum.Enum` (good for Python too) |
| String-to-class card factory | Refactor to explicit `match`/`case` factory |
| Card polymorphism | `std::vector<Card*>` + virtual methods — trivial |
| Agent polymorphism | `Agent*` — trivial |
| Callbacks in `Choice` / `kt.Card` | `std::function` with pointer capture — fine for gameplay |
| Draw callbacks capturing game state | `Game_State*` — fine as long as lifetime is managed |
| Optional / None values | Sentinel values or `std::optional` — mechanical |
| Animated cards deep copy | Plain copy + init flag — trivial |

## Pre-Port Refactors (Recommended Order)

1. **Capture card indices in closures, not card pointers** — the only fix that affects correctness (MCTS). In every `on_played` closure, replace `captured_card = self` with `captured_index = self.id` and look up via `state.all_cards[captured_index]`.
2. **Replace string areas with `Area` enum** — `Card_Id.area: Area`, `Choice.description: Choice_Kind`.
3. **Replace `isinstance` dispatch with `match`/`case`** — cleaner and maps directly to C++ `std::visit`.
4. **Replace string-to-class card factory with explicit `match`/`case` factory function.**
