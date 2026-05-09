"""Re-exports of the C++ gameplay helpers and AI agent."""

from tressette.game._tressette_cpp import (  # noqa: F401
    Tressette_Agent,
    card_thirds,
    compute_player_score,
    play_card,
    strength,
    trick_winner,
)
