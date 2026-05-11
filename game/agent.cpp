#include "agent.h"

int Agent_Random::choose_action(Game& state, const Choice& choice) {
  int n = action_options_count(choice.actions(state));
  if (n <= 0) return 0;
  std::uniform_int_distribution<int> dist(0, n - 1);
  return dist(rng);
}
