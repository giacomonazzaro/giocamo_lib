// Times the search on a set of mid-game positions, so a change to it can be
// compared against the version before it.
//
// Run from the repository root, so that mindbug/cards.json is found:
//   compile mindbug mindbug_benchmark

#include <game/agent.h>
#include <game/minimax.h>
#include <mindbug/gameplay.h>

#include <chrono>
#include <cstdio>

using namespace mindbug;
using Clock = std::chrono::steady_clock;

// The settings the app plays with.
constexpr int MAX_DEPTH   = 13;
constexpr int NUM_SAMPLES = 15;

// Positions to time: a few deals, each played forward by random moves so the
// board is not empty, then a few searched decisions each.
constexpr int NUM_DEALS       = 4;
constexpr int OPENING_MOVES   = 12;
constexpr int TIMED_DECISIONS = 3;

int main() {
  if (!load_card_designs()) {
    std::fprintf(stderr, "run mindbug_benchmark from the repository root\n");
    return 1;
  }

  auto  agent    = Agent_Minimax_Stochastic<Game_State>(MAX_DEPTH, NUM_SAMPLES);
  float total    = 0.0f;
  float slowest  = 0.0f;
  int   decisions = 0;

  for (int deal = 0; deal < NUM_DEALS; ++deal) {
    auto state        = quick_setup(deal);
    auto random_agent = Agent_Random(deal);
    for (int move = 0; move < OPENING_MOVES && !state.is_game_over(); ++move) {
      resolve_choice(
        state, random_agent.choose_action(state, pending_choice(state))
      );
    }

    for (int move = 0; move < TIMED_DECISIONS && !state.is_game_over(); ++move) {
      const auto started = Clock::now();
      const int  action  = agent.choose_action(state, pending_choice(state));
      const float seconds =
        std::chrono::duration<float>(Clock::now() - started).count();
      total += seconds;
      slowest = std::max(slowest, seconds);
      decisions += 1;
      resolve_choice(state, action);
    }
  }

  std::fprintf(
    stderr,
    "BENCHMARK %d decisions, %.2fs total, %.3fs mean, %.3fs slowest\n",
    decisions,
    total,
    total / (float)decisions,
    slowest
  );
  return 0;
}
