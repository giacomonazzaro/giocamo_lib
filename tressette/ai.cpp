#include "ai.h"

#include <algorithm>

#include "gameplay.h"

namespace tressette {

float evaluate_state(Game_State& game, int player_index) {
  int   my  = compute_player_score(game, player_index);
  int   opp = compute_player_score(game, 1 - player_index);
  float s   = float(my - opp);
  if (game.is_game_over()) {
    if (my > opp) return +1000.0f;
    if (my < opp) return -1000.0f;
    return 0.0f;
  }
  return s;
}

Game_State sample_state(
  const Game_State& state, int player_index, std::mt19937& rng
) {
  Game_State sampled        = state;
  const int  opponent_index = 1 - player_index;
  Player&    opponent       = sampled.players[opponent_index];
  const int  hand_size      = (int)opponent.hand.size();

  std::vector<int> hidden(opponent.hand.begin(), opponent.hand.end());
  hidden.insert(hidden.end(), sampled.stock.begin(), sampled.stock.end());
  std::shuffle(hidden.begin(), hidden.end(), rng);

  opponent.hand.assign(hidden.begin(), hidden.begin() + hand_size);
  sampled.stock.assign(hidden.begin() + hand_size, hidden.end());
  return sampled;
}

}  // namespace tressette
