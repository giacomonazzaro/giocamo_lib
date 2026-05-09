"""Python shim — re-exports gods gameplay + AI API from gods._gods_cpp."""

from gods._gods_cpp import (  # noqa: F401
    Agent_Minimax_Stochastic_Gods,
    compute_player_score,
    destroy_people,
    destroy_wonder,
    discard_cards,
    draw_card,
    make_claim_choice,
    make_main_choice,
    play_card,
    restore_people,
    shuffle_card_into_deck,
    wonders_by_priority,
)
