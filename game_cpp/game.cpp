#include "game.h"

#include "agent.h"

namespace {

// Binomial coefficient C(n, k). Iterative, no overflow guard for typical hand sizes.
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
      if constexpr (std::is_same_v<T, Choose_Card> || std::is_same_v<T, Choose_Option>) {
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

void resolve_choice(Game& game, const Choice& choice, int index) {
  std::vector<Choice> new_choices = choice.resolve(game, index);
  game.choices.insert(
    game.choices.end(),
    std::make_move_iterator(new_choices.begin()),
    std::make_move_iterator(new_choices.end())
  );
}

void game_loop(Game& game, Agent& agent, std::function<void(Game&)> callback) {
  while (!game.is_game_over()) {
    std::optional<Choice> choice = game.next_choice();
    if (!choice) break;

    int index = agent.choose_action(game, *choice);
    resolve_choice(game, *choice, index);
  }

  if (callback) callback(game);
}

std::optional<Choice> game_frame(Game& game, Agent& agent, std::optional<Choice> choice) {
  // Only fetch a new choice when the previous one has been resolved.
  if (!choice) {
    choice = game.next_choice();
  }

  if (choice) {
    if (action_options_count(choice->actions(game)) == 0) {
      return std::nullopt;
    }
    int action_index = agent.choose_action(game, *choice);
    if (action_index != -1) {
      resolve_choice(game, *choice, action_index);
      choice = std::nullopt;
    }
  }

  return choice;
}
