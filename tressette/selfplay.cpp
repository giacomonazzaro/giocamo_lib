// Headless self-play data generator for Tressette.
// Runs N games of Tressette_Agent vs Tressette_Agent and dumps every
// pre-action state plus final scores to a compact binary file.
// See plan: ../../.claude/plans/create-a-c-program-tranquil-thacker.md.

#include <game/agent.h>
#include <game/game.h>
#include <tressette/ai.h>
#include <tressette/gameplay.h>
#include <tressette/models.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

// Binary file constants. Must match the PyTorch loader in train_value.py.
static constexpr std::uint32_t MAGIC         = 0x54525353u;  // 'TRSS' (little-endian).
static constexpr std::uint32_t FORMAT_VERSION = 1u;
static constexpr int           RECORD_BYTES   = 36;

// One snapshot = the game state just before a single action is taken.
struct Snapshot {
  std::uint64_t hand_player_0;
  std::uint64_t hand_player_1;
  std::uint64_t captured_player_0;
  std::uint64_t captured_player_1;
  std::uint8_t  current_player;
};

// Pack a list of card ids (0..39) into a 40-bit bitmask.
static inline std::uint64_t card_ids_to_bitmask(const std::vector<int>& ids) {
  std::uint64_t mask = 0;
  for (int id : ids) mask |= (std::uint64_t(1) << id);
  return mask;
}

static inline Snapshot make_snapshot(const tressette::Game_State& state) {
  auto snap              = Snapshot();
  snap.hand_player_0     = card_ids_to_bitmask(state.players[0].hand);
  snap.hand_player_1     = card_ids_to_bitmask(state.players[1].hand);
  snap.captured_player_0 = card_ids_to_bitmask(state.players[0].tricks_won);
  snap.captured_player_1 = card_ids_to_bitmask(state.players[1].tricks_won);
  snap.current_player    = static_cast<std::uint8_t>(state.current_player);
  return snap;
}

// Serialize one record into a 36-byte little-endian buffer.
// Layout: 4 x uint64 bitmasks, then current_player, score_0, score_1, padding.
static inline void write_record(
  std::ostream& out,
  const Snapshot& snap,
  std::uint8_t final_score_player_0,
  std::uint8_t final_score_player_1
) {
  char buf[RECORD_BYTES];
  std::memcpy(buf +  0, &snap.hand_player_0,     8);
  std::memcpy(buf +  8, &snap.hand_player_1,     8);
  std::memcpy(buf + 16, &snap.captured_player_0, 8);
  std::memcpy(buf + 24, &snap.captured_player_1, 8);
  buf[32] = static_cast<char>(snap.current_player);
  buf[33] = static_cast<char>(final_score_player_0);
  buf[34] = static_cast<char>(final_score_player_1);
  buf[35] = 0;  // Padding to keep records 8-byte aligned.
  out.write(buf, RECORD_BYTES);
}

// Parse a CLI flag like "--name=value". Returns true if matched.
static bool parse_int_flag(
  const std::string& arg, const std::string& name, int& out
) {
  std::string prefix = "--" + name + "=";
  if (arg.rfind(prefix, 0) != 0) return false;
  out = std::atoi(arg.c_str() + prefix.size());
  return true;
}
static bool parse_str_flag(
  const std::string& arg, const std::string& name, std::string& out
) {
  std::string prefix = "--" + name + "=";
  if (arg.rfind(prefix, 0) != 0) return false;
  out = arg.substr(prefix.size());
  return true;
}

int main(int argc, char** argv) {
  // Defaults match the plan.
  int         num_games   = 1000;
  int         depth       = 6;
  int         samples     = 20;
  int         seed        = 42;
  std::string out_path    = "selfplay_data.bin";

  for (int i = 1; i < argc; ++i) {
    auto a = std::string(argv[i]);
    if (parse_int_flag(a, "num-games", num_games)) continue;
    if (parse_int_flag(a, "depth",     depth))     continue;
    if (parse_int_flag(a, "samples",   samples))   continue;
    if (parse_int_flag(a, "seed",      seed))      continue;
    if (parse_str_flag(a, "out",       out_path))  continue;
    std::cerr << "unknown argument: " << a << "\n";
    return 1;
  }

  std::cerr << "tressette_selfplay: games=" << num_games
            << " depth=" << depth
            << " samples=" << samples
            << " seed=" << seed
            << " out=" << out_path << "\n";

  // Reserve header space; we'll seek back and fill it once the run is done
  // (num_snapshots is only known after all games have played).
  std::ofstream out(out_path, std::ios::binary);
  if (!out) {
    std::cerr << "failed to open output file\n";
    return 1;
  }
  char header_placeholder[16] = {0};
  out.write(header_placeholder, sizeof(header_placeholder));

  std::uint32_t total_snapshots = 0;

  for (int game_index = 0; game_index < num_games; ++game_index) {
    // Distinct seed per game so the dataset isn't all the same deal.
    int                   game_seed = seed + game_index;
    tressette::Game_State state     = tressette::quick_setup(game_seed);

    // Two independent agent instances (the minimax agent is stateless aside
    // from its thread-local rng, but using two makes the intent explicit).
    auto agent_0 = tressette::Tressette_Agent(depth, samples);
    auto agent_1 = tressette::Tressette_Agent(depth, samples);
    auto duel    = Agent_Duel(&agent_0, &agent_1, /*swap=*/false);

    std::vector<Snapshot> snapshots;
    snapshots.reserve(40);

    while (!state.game_over) {
      std::optional<Choice> choice = state.next_choice();
      if (!choice) break;

      // Snapshot the state the agent is about to act on.
      snapshots.push_back(make_snapshot(state));

      int action_index = duel.choose_action(state, *choice);
      if (action_index < 0) break;  // Defensive: minimax always returns >= 0.
      resolve_choice(state, *choice, action_index);
    }

    int score_0_int = tressette::compute_player_score(state, 0);
    int score_1_int = tressette::compute_player_score(state, 1);
    auto score_0    = static_cast<std::uint8_t>(score_0_int);
    auto score_1    = static_cast<std::uint8_t>(score_1_int);

    for (const Snapshot& snap : snapshots) {
      write_record(out, snap, score_0, score_1);
    }
    total_snapshots += static_cast<std::uint32_t>(snapshots.size());

    std::cerr << "game " << (game_index + 1) << "/" << num_games
              << "  snapshots=" << snapshots.size()
              << "  score=" << score_0_int << "-" << score_1_int << "\n";
  }

  // Backfill the header now that we know num_snapshots.
  out.seekp(0, std::ios::beg);
  std::uint32_t header[4] = {
    MAGIC, FORMAT_VERSION, static_cast<std::uint32_t>(num_games), total_snapshots
  };
  out.write(reinterpret_cast<const char*>(header), sizeof(header));
  out.close();

  std::cerr << "done. total_snapshots=" << total_snapshots
            << "  file=" << out_path << "\n";
  return 0;
}
