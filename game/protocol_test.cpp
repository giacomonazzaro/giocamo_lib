// The app loop and a search must walk the same sequence of choices.
//
// A game may build a choice by changing itself. Such a builder returns a
// different choice on every call. A caller that asks the builder for the
// pending choice therefore skips whatever resolve had already produced. The
// game below builds choices by draining a queue, so it shows that skip.

#include <cstdio>
#include <string>
#include <vector>

#include "agent.h"
#include "game.h"
#include "minimax.h"

// Six turns. Every other turn also queues one follow-up choice, so half the
// game's decisions only exist in the queue.
struct Queue_Game : Game {
  int turn = 0;
  // Follow-up choices produced by a turn, drained one per next_choice().
  std::vector<std::string> queue;
  // Every choice this game has handed out, in order.
  std::vector<std::string> handed_out;

  bool is_game_over() const override { return turn >= 6; }

  Choice next_choice() override {
    if (is_game_over()) return {};
    if (!queue.empty()) {
      std::string description = queue.front();
      queue.erase(queue.begin());
      return make_choice(description);
    }
    return make_choice("turn");
  }

  Choice make_choice(const std::string& description) {
    handed_out.push_back(description);

    Choice choice;
    choice.player_index = turn % 2;
    choice.description  = description == "turn" ? "turn" : "follow-up";
    choice.actions      = [](Game&) -> Choose {
      Choose_Option options;
      options.targets = {"A", "B"};
      return options;
    };
    choice.resolve = [](Game& game, int) -> Choice {
      Queue_Game& queue_game = static_cast<Queue_Game&>(game);
      queue_game.turn += 1;
      if (queue_game.turn % 2 == 1) queue_game.queue.push_back("follow-up");
      return null_choice;
    };
    return choice;
  }
};

float evaluate_state(const Queue_Game& game, int) { return (float)game.turn; }

// Plays a whole game the way the app's frame loop does.
static std::vector<std::string> walk_with_game_frame() {
  Queue_Game               game;
  Agent_Random             agent(1);
  std::vector<std::string> seen;
  game.begin_game();
  while (!game.is_game_over()) {
    seen.push_back(std::string(pending_choice(game).description));
    game_frame(game, agent);
  }
  return seen;
}

// Plays a whole game the way minimax and mcts step through it.
static std::vector<std::string> walk_like_search() {
  Queue_Game               game;
  Agent_Random             agent(1);
  std::vector<std::string> seen;
  game.begin_game();
  while (!game.is_game_over()) {
    if (pending_action_count(game) == 0) break;
    seen.push_back(std::string(pending_choice(game).description));
    resolve_choice(game, agent.choose_action(game, pending_choice(game)));
  }
  return seen;
}

static void print_choices(
  const char* label, const std::vector<std::string>& c
) {
  std::printf("  %s (%d):", label, (int)c.size());
  for (const std::string& description : c) {
    std::printf(" %s", description.c_str());
  }
  std::printf("\n");
}

int main() {
  bool all_ok = true;

  std::vector<std::string> loop   = walk_with_game_frame();
  std::vector<std::string> search = walk_like_search();
  print_choices("app loop", loop);
  print_choices("search  ", search);
  if (loop != search) {
    std::printf("FAIL: the two paths walk different choices.\n");
    all_ok = false;
  }
  // The follow-up choices are the ones a second next_choice() call would eat.
  int follow_ups = 0;
  for (const std::string& description : loop) {
    if (description == "follow-up") follow_ups += 1;
  }
  if (follow_ups == 0) {
    std::printf("FAIL: no queued choices, the test proves nothing.\n");
    all_ok = false;
  }

  // Thinking must not advance the game. A search that calls next_choice() on
  // the caller's state instead of reading its pending choice drains the queue
  // behind the caller's back, and the choice it just ate is never presented.
  Queue_Game game;
  game.queue.push_back("follow-up");
  game.begin_game();
  const std::string choice_before =
    std::string(pending_choice(game).description);
  const int                 turn_before   = game.turn;
  const int                 handed_before = (int)game.handed_out.size();
  Agent_Minimax<Queue_Game> minimax(6, 1);
  minimax.choose_action(game, pending_choice(game));
  if (std::string(pending_choice(game).description) != choice_before ||
      game.turn != turn_before ||
      (int)game.handed_out.size() != handed_before) {
    std::printf("FAIL: the search advanced the game it was asked about.\n");
    all_ok = false;
  }

  std::printf(all_ok ? "protocol test passed\n" : "protocol test FAILED\n");
  return all_ok ? 0 : 1;
}
