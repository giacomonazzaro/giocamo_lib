#include "ai.h"

#include <algorithm>

#include "gameplay.h"

namespace tressette {

// When true, evaluate_state returns a reward in [0,1] (0.5 = even) — the range
// UCB1's exploration constant assumes; when false it uses the raw scale. They
// test about even (match_mcts), and since rollouts run to the end of the game
// the mid-game value is rarely reached anyway. Default raw — the scale the
// softmax rollout guidance is tuned for.
bool use_normalized_evaluation = false;

float evaluate_state(Game_State& game, int player_index) {
  const bool over       = game.is_game_over();
  const int  my_thirds  = compute_player_thirds(game, player_index);
  const int  opp_thirds = compute_player_thirds(game, 1 - player_index);

  if (use_normalized_evaluation) {
    // [0,1] from player_index's perspective: 1 = win, 0 = loss, 0.5 = even.
    if (over) {
      int my  = compute_player_score(game, player_index);
      int opp = compute_player_score(game, 1 - player_index);
      if (my > opp) return 1.0f;
      if (my < opp) return 0.0f;
      return 0.5f;
    }
    // Mid-game (rarely reached): each player's captured points over the 11 at
    // stake, combined into a [0,1] differential.
    float my_points  = (my_thirds / 3.0f) / 11.0f;
    float opp_points = (opp_thirds / 3.0f) / 11.0f;
    return 0.5f + 0.5f * (my_points - opp_points);
  }

  // Legacy raw scale: large terminal magnitude, thirds/3 mid-game. Using
  // un-floored thirds gives every captured card an immediate, proportional
  // effect instead of only registering at 3-thirds boundaries.
  if (over) {
    int my  = compute_player_score(game, player_index);
    int opp = compute_player_score(game, 1 - player_index);
    if (my > opp) return +1000.0f;
    if (my < opp) return -1000.0f;
    return 0.0f;
  }
  return (float)(my_thirds - opp_thirds) / 3.0f;
}

Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
) {
  Game_State sampled        = state;
  const int  opponent_index = 1 - player_index;
  Player&    opponent       = sampled.players[opponent_index];
  const int  hand_size      = (int)opponent.hand.size();

  // Opponent's hand plus the stock are the cards hidden from `player_index`:
  // at most 10 + 20 = 30, so they live inline.
  auto hidden = Inlined_Vector<int, 30>(opponent.hand);
  hidden.insert(hidden.end(), sampled.stock.begin(), sampled.stock.end());
  std::shuffle(hidden.begin(), hidden.end(), rng);

  opponent.hand.assign(hidden.begin(), hidden.begin() + hand_size);
  sampled.stock.assign(hidden.begin() + hand_size, hidden.end());
  return sampled;
}

}  // namespace tressette
