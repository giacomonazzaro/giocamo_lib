#pragma once

#include <functional>
#include <random>
#include <utility>
#include <vector>

#include "agent.h"
#include "game.h"

#ifndef __EMSCRIPTEN__
#include <thread>
#endif

// Root sampling for a game with hidden information.
//
// The searching player cannot see the opponent's hand, so searching the real
// position is not possible. Instead the hidden cards are shuffled into a deal
// the player cannot tell apart from it, an agent picks its move there, and the
// move that wins the vote across many such deals is played.
//
// Any agent can be the one asked — minimax, MCTS, anything that answers a
// choice — so the sampling lives here once instead of inside each of them. The
// game must provide:
//
//   Game_T sample_state(const Game_T& state, int player_index, std::mt19937&);
//
// `make_inner` is called once per sample, so every sample gets its own agent
// and the threads share nothing.
template <class Game_T, class Inner_Agent_T>
struct Agent_Stochastic : Agent {
  std::function<Inner_Agent_T()> make_inner;
  int                            num_samples;
  // Where the sampled deals come from. Fixed, so the same position searched
  // again gives the same answer, and a change to the search can be measured.
  unsigned int sampling_seed = 1;

#ifdef __EMSCRIPTEN__
  // The browser has one thread and it is the one drawing, so the deals are
  // sampled one per frame: a call answers -1, which the game loop reads as
  // "not ready, ask again next frame", and the page keeps rendering in
  // between. The votes gathered so far live here until they are all in.
  int              samples_done = 0;
  std::vector<int> votes_so_far;
#endif

  Agent_Stochastic(
    std::function<Inner_Agent_T()> make_inner, int num_samples = 20
  )
      : make_inner(std::move(make_inner)), num_samples(num_samples) {}

  void message(const std::string&) override {}

  int choose_action(Game& state, const Choice& choice) override {
    Game_T&   concrete     = static_cast<Game_T&>(state);
    const int num_actions  = pending_action_count(state);
    const int player_index = pending_choice(state).player_index;
    if (num_actions <= 0) return 0;
    if (num_actions == 1) return 0;

    // A deal per sample, each from its own generator so a thread never shares
    // one and the whole thing stays reproducible.
    auto deal_for = [&](int sample) {
      auto rng =
        std::mt19937(sampling_seed * 2654435761u + (unsigned int)sample);
      return sample_state(concrete, player_index, rng);
    };

#ifdef __EMSCRIPTEN__
    if (samples_done == 0) votes_so_far = std::vector<int>(num_actions, 0);

    Game_T sampled = deal_for(samples_done);
    auto   inner   = make_inner();
    // The agent may itself want more frames; ask it again next frame, on the
    // same deal, until it answers.
    const int action = inner.choose_action(sampled, choice);
    if (action < 0) return -1;

    votes_so_far[action] += 1;
    samples_done += 1;
    if (samples_done < num_samples) return -1;

    samples_done = 0;
    sampling_seed += 1;
    return (int)argmax_randomized(votes_so_far);
#else
    // The deals are independent, so they are searched at the same time. Each
    // thread writes one entry of `picks`, so nothing needs locking.
    auto picks   = std::vector<int>(num_samples, -1);
    auto threads = std::vector<std::thread>(num_samples);
    for (int sample = 0; sample < num_samples; ++sample) {
      threads[sample] = std::thread([&, sample] {
        Game_T sampled = deal_for(sample);
        auto   inner   = make_inner();
        picks[sample]  = inner.choose_action(sampled, choice);
      });
    }
    for (auto& thread : threads) thread.join();

    auto votes = std::vector<int>(num_actions, 0);
    for (int pick : picks) {
      if (pick >= 0) votes[pick] += 1;
    }
    sampling_seed += 1;
    return (int)argmax_randomized(votes);
#endif
  }
};
