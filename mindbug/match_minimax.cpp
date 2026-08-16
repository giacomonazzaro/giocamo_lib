// Headless match: alpha-beta minimax against itself at two different depths,
// to see what one more ply is worth. Both sides read the whole position,
// hidden cards included, so this measures the search and nothing else.
//
// Run from the repository root, where mindbug/cards.json is.

#include <game/agent.h>
#include <game/game.h>
#include <game/minimax.h>
#include <mindbug/gameplay.h>
#include <mindbug/models.h>

#include <cstdlib>
#include <iostream>
#include <string>

static bool parse_int_flag(
  const std::string& arg, const std::string& name, int& out
) {
  std::string prefix = "--" + name + "=";
  if (arg.rfind(prefix, 0) != 0) return false;
  out = std::atoi(arg.c_str() + prefix.size());
  return true;
}

int main(int argc, char** argv) {
  int num_games = 20;
  int depth_a   = 10;
  int depth_b   = 9;
  int seed      = 42;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (parse_int_flag(arg, "num-games", num_games)) continue;
    if (parse_int_flag(arg, "depth-a", depth_a)) continue;
    if (parse_int_flag(arg, "depth-b", depth_b)) continue;
    if (parse_int_flag(arg, "seed", seed)) continue;
    std::cerr << "unknown argument: " << arg << "\n";
    return 1;
  }

  if (!mindbug::load_card_designs()) {
    std::cerr << "run mindbug_match_minimax from the repository root\n";
    return 1;
  }

  std::cerr << "mindbug_match_minimax: games=" << num_games << "  seed=" << seed
            << "\n  depth " << depth_a << "  vs  depth " << depth_b << "\n";

  auto agent_a = Agent_Minimax_Stochastic<mindbug::Game_State>(depth_a, 16);
  auto agent_b = Agent_Minimax_Stochastic<mindbug::Game_State>(depth_b, 16);

  auto make_state = [&](int game_index) {
    return mindbug::quick_setup(seed + game_index);
  };
  auto score = [](const mindbug::Game_State& game, int player_index) {
    return mindbug::compute_player_score(game, player_index);
  };

  std::string name_a = "depth-" + std::to_string(depth_a);
  std::string name_b = "depth-" + std::to_string(depth_b);

  Benchmark_Result result = benchmark_agents<mindbug::Game_State>(
    num_games,
    make_state,
    score,
    agent_a,
    name_a.c_str(),
    agent_b,
    name_b.c_str()
  );

  std::cout << "\nresult:  " << name_a << "_wins=" << result.a_wins << "  "
            << name_b << "_wins=" << result.b_wins << "  draws=" << result.draws
            << "\n";
  std::cout << "compute (wall clock):\n";
  std::cout << "  " << name_a << ": avg_per_move=" << result.a_ms_per_move()
            << "ms\n";
  std::cout << "  " << name_b << ": avg_per_move=" << result.b_ms_per_move()
            << "ms\n";
  return 0;
}
