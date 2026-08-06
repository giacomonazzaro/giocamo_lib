#include "game.h"

#include <cassert>

#include "agent.h"

namespace {

// Binomial coefficient C(n, k). Iterative, no overflow guard for typical hand
// sizes.
long long binomial(int n, int k) {
  if (k < 0 || k > n) return 0;
  if (k == 0 || k == n) return 1;
  if (k > n - k) k = n - k;
  long long result = 1;
  for (int i = 0; i < k; ++i) {
    result = result * (n - i) / (i + 1);
  }
  return result;
}

// Sum of C(n, k) for k in [0..count], inclusive.
long long binomial_up_to(int n, int count) {
  long long total = 0;
  for (int k = 0; k <= count; ++k) {
    total += binomial(n, k);
  }
  return total;
}

}  // namespace

int action_options_count(const Choose& choose) {
  return std::visit(
    [](const auto& c) -> int {
      using T = std::decay_t<decltype(c)>;
      if constexpr (std::is_same_v<T, Choose_Card> ||
                    std::is_same_v<T, Choose_Option>) {
        return static_cast<int>(c.targets.size());
      } else {
        // Choose_Cards / Choose_Options: enumerate valid combinations.
        const int n = static_cast<int>(c.targets.size());
        if (c.up_to) {
          return static_cast<int>(binomial_up_to(n, c.count));
        }
        if (n <= c.count) return 1;
        return static_cast<int>(binomial(n, c.count));
      }
    },
    choose
  );
}

int pending_action_count(Game& game) {
  if (!game._choice.actions) return 0;
  return action_options_count(game._choice.actions(game));
}

void resolve_choice(Game& game, int index) {
  Choice next = game._choice.resolve(game, index);
  // The resolve could not say what comes next, so ask the game. This is the
  // only call to next_choice() once a game is running, which is what keeps the
  // app loop and the searches on the same sequence of choices.
  if (next.is_null()) next = game.next_choice();
  game._choice = std::move(next);
}

void game_loop(Game& game, Agent& agent, std::function<void(Game&)> callback) {
  while (!game.is_game_over()) game_frame(game, agent);
  if (callback) callback(game);
}

bool game_frame(Game& game, Agent& agent) {
  assert(!game.is_game_over());
  // resolve_choice already refilled the pending choice, so there is always a
  // decision to present here.
  assert(pending_action_count(game) > 0);

  int action_index = agent.choose_action(game, game._choice);
  if (action_index == -1) {
    return false;  // Agent not ready yet, try again next frame.
  }
  resolve_choice(game, action_index);
  return true;
}
