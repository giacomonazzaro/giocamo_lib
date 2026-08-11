#include "gameplay.h"

#include <cassert>

#include "cards.h"

// namespace mindbug {

// ---- Queries ----

int effective_power(const Game_State& state, int card) {
  const int     controller = controller_of(state, card);
  const bool    its_turn   = state.current_player == controller;
  const int     design     = design_of(state, card);
  const Player& player     = state.players[controller];
  int           power      = card_designs[design].power;

  for (int ally : player.creatures) {
    if (ally == card) continue;
    const int ally_design = design_of(state, ally);
    if (ally_design == SHIELD_BUGS) power += 1;
    if (ally_design == URCHIN_HURLER && its_turn) power += 2;
  }
  if (design == GOBLIN_WEREWOLF && its_turn) power += 6;
  if (design == LONE_YETI && player.creatures.size() == 1) power += 5;
  return power;
}

// mirror=false stops Sharky from copying another Sharky's copied keywords.
static int keywords_of(const Game_State& state, int card, bool mirror) {
  const int     controller = controller_of(state, card);
  const int     design     = design_of(state, card);
  const Player& player     = state.players[controller];
  int           keywords   = card_designs[design].keywords;

  if (design == LONE_YETI && player.creatures.size() == 1) keywords |= FRENZY;
  if (effective_power(state, card) <= 4) {
    for (int ally : player.creatures) {
      if (ally != card && design_of(state, ally) == SNAIL_THROWER) {
        keywords |= HUNTER | POISONOUS;
      }
    }
  }
  if (design == SHARKY_CRAB_DOG_MUMMYPUS && mirror) {
    for (int enemy : state.players[1 - controller].creatures) {
      keywords |= keywords_of(state, enemy, false) &
                  (HUNTER | SNEAKY | FRENZY | POISONOUS);
    }
  }
  return keywords;
}

int effective_keywords(const Game_State& state, int card) {
  return keywords_of(state, card, true);
}

std::vector<int> creature_targets(
  const Game_State& state, int controller, int min_power, int max_power
) {
  auto targets = std::vector<int>();
  for (int player = 0; player < 2; ++player) {
    if (controller != -1 && player != controller) continue;
    for (int card : state.players[player].creatures) {
      const int power = effective_power(state, card);
      if (power < min_power || power > max_power) continue;
      targets.push_back(card);
    }
  }
  return targets;
}

bool can_block(const Game_State& state, int attacker, int blocker) {
  if (effective_keywords(state, attacker) & SNEAKY) {
    if (!(effective_keywords(state, blocker) & SNEAKY)) return false;
  }
  const int power = effective_power(state, blocker);
  if (design_of(state, attacker) == BEE_BEAR && power <= 6) return false;

  if (power <= 4) {
    const int controller = controller_of(state, attacker);
    for (int ally : state.players[controller].creatures) {
      if (design_of(state, ally) == ELEPHANTOPUS) return false;
    }
  }
  return true;
}

std::vector<Turn_Action> turn_actions(const Game_State& state) {
  auto          actions = std::vector<Turn_Action>();
  const Player& player  = state.players[state.current_player];
  for (int card : player.hand) actions.push_back(Turn_Action{false, card});
  for (int card : player.creatures) actions.push_back(Turn_Action{true, card});
  return actions;
}

int compute_player_score(const Game_State& state, int player) {
  return state.winner == player ? 1 : 0;
}

// ---- Choice helpers ----

std::vector<std::vector<int>> target_combinations(
  const std::vector<int>& targets, int count, bool up_to
) {
  const int size   = (int)targets.size();
  auto      result = std::vector<std::vector<int>>();
  if (!up_to && size <= count) {
    result.push_back(targets);
    return result;
  }
  const int first = up_to ? 0 : count;
  for (int k = first; k <= std::min(count, size); ++k) {
    if (k == 0) {
      result.push_back({});
      continue;
    }
    auto mask = std::vector<bool>(size, false);
    std::fill(mask.end() - k, mask.end(), true);
    do {
      auto selection = std::vector<int>();
      for (int i = 0; i < size; ++i) {
        if (mask[i]) selection.push_back(targets[i]);
      }
      result.push_back(selection);
    } while (std::next_permutation(mask.begin(), mask.end()));
  }
  return result;
}

Choice make_choice(
  int                                          player,
  const char*                                  description,
  std::function<std::vector<int>(Game_State&)> get_targets,
  std::function<void(Game_State&, int)>        on_chosen
) {
  auto choice             = Choice();
  choice.player_index     = player;
  choice.description      = description;
  choice.text_description = description;
  choice.actions          = [get_targets](Game& game) -> Choose {
    return Choose_Card{get_targets(static_cast<Game_State&>(game)), false};
  };
  choice.resolve = [get_targets, on_chosen](Game& game, int index) -> Choice {
    Game_State& state = static_cast<Game_State&>(game);
    on_chosen(state, get_targets(state)[index]);
    return null_choice;
  };
  return choice;
}

Choice make_multi_choice(
  int                                                       player,
  const char*                                               description,
  std::function<std::vector<int>(Game_State&)>              get_targets,
  int                                                       count,
  bool                                                      up_to,
  std::function<void(Game_State&, const std::vector<int>&)> on_chosen
) {
  auto choice             = Choice();
  choice.player_index     = player;
  choice.description      = description;
  choice.text_description = description;
  choice.actions          = [get_targets, count, up_to](Game& game) -> Choose {
    return Choose_Cards{
      get_targets(static_cast<Game_State&>(game)), count, up_to
    };
  };
  choice.resolve =
    [get_targets, count, up_to, on_chosen](Game& game, int index) -> Choice {
    Game_State& state = static_cast<Game_State&>(game);
    on_chosen(
      state, target_combinations(get_targets(state), count, up_to)[index]
    );
    return null_choice;
  };
  return choice;
}

// ---- Mechanics ----

static void end_game(Game_State& state, int winner) {
  state.game_over = true;
  state.winner    = winner;
  state.queue.clear();
}

static void add_creature(Game_State& state, int card, int controller) {
  // Already in play?
  if (controller_of(state, card) != -1) return;
  state.players[controller].creatures.push_back(card);
}

void lose_life(Game_State& state, int player, int amount) {
  state.players[player].life -= amount;
  if (state.players[player].life <= 0) end_game(state, 1 - player);
}

void take_control(Game_State& state, int card, int controller) {
  remove_card(state.players[1 - controller].creatures, card);
  add_creature(state, card, controller);
}

void enter_play(Game_State& state, int card, int controller) {
  add_creature(state, card, controller);
  remove_card(state.exhausted_cards, card);  // Tough starts over.

  // A Deathweaver on the other side switches the Play ability off.
  for (int enemy : state.players[1 - controller].creatures) {
    if (design_of(state, enemy) == DEATHWEAVER) return;
  }
  trigger_play(state, card);
}

void defeat_creature(Game_State& state, int card) {
  const int controller = controller_of(state, card);
  if (controller == -1) return;
  if ((effective_keywords(state, card) & TOUGH) && !is_exhausted(state, card)) {
    state.exhausted_cards.push_back(card);
    return;
  }
  remove_card(state.players[controller].creatures, card);
  state.players[controller].discard.push_back(card);
  trigger_defeated(state, card, controller);
}

// ---- Phases ----

static Choice make_turn_choice(Game_State& state) {
  auto choice             = Choice();
  choice.player_index     = state.current_player;
  choice.description      = "turn";
  choice.text_description = "Play a creature or attack with one";
  choice.actions          = [](Game& game) -> Choose {
    auto options = Choose_Card();
    for (const Turn_Action& action :
         turn_actions(static_cast<Game_State&>(game))) {
      options.targets.push_back(pack_turn_action(action));
    }
    options.up_to = false;
    return options;
  };
  choice.resolve = [](Game& game, int index) -> Choice {
    Game_State&       state  = static_cast<Game_State&>(game);
    const Turn_Action action = turn_actions(state)[index];
    if (action.is_attack) {
      state.attacker     = action.card;
      state.attack_count = 0;
      state.phase        = Phase::ATTACK;
    } else {
      state.played_card = action.card;
      remove_card(state.active_player().hand, action.card);
      draw_back_up_to_hand_size(state);
      state.phase = Phase::MINDBUG;
    }
    return null_choice;
  };
  return choice;
}

// Put the creature the active player is playing into play, stolen by the
// opponent or not, and end the turn. Stealing gives the active player another
// turn.
static void resolve_played_creature(Game_State& state, bool stolen) {
  const int thief   = 1 - state.current_player;
  const int card    = state.played_card;
  state.played_card = -1;
  state.phase       = Phase::TURN_END;
  if (stolen) {
    state.players[thief].mindbugs -= 1;
    state.extra_turn = true;
  }
  enter_play(state, card, stolen ? thief : state.current_player);
}

static Choice make_mindbug_choice(Game_State& state) {
  auto choice             = Choice();
  choice.player_index     = 1 - state.current_player;
  choice.description      = "mindbug";
  choice.text_description = "Use a Mindbug to take this creature?";
  choice.actions          = [](Game&) -> Choose {
    return Choose_Option{{"Use Mindbug", "Pass"}};
  };
  choice.resolve = [](Game& game, int index) -> Choice {
    resolve_played_creature(static_cast<Game_State&>(game), index == 0);
    return null_choice;
  };
  return choice;
}

// A frenzy creature that survived its first attack may attack again, and its
// controller decides whether it does.
static Choice make_frenzy_choice(Game_State& state) {
  auto choice             = Choice();
  choice.player_index     = controller_of(state, state.attacker);
  choice.description      = "frenzy";
  choice.text_description = "Attack a second time?";
  choice.actions          = [](Game&) -> Choose {
    return Choose_Option{{"Attack again", "End turn"}};
  };
  choice.resolve = [](Game& game, int index) -> Choice {
    Game_State& state = static_cast<Game_State&>(game);
    state.phase       = index == 0 ? Phase::ATTACK : Phase::TURN_END;
    return null_choice;
  };
  return choice;
}

static std::vector<int> legal_blockers(const Game_State& state) {
  const int defender = 1 - controller_of(state, state.attacker);
  auto      blockers = std::vector<int>();
  for (int candidate : state.players[defender].creatures) {
    if (can_block(state, state.attacker, candidate)) {
      blockers.push_back(candidate);
    }
  }
  return blockers;
}

static Choice make_block_choice(Game_State& state) {
  // A hunter's controller picks the blocker, unless they pass the decision
  // back; then the defender chooses as usual.
  const int  controller = controller_of(state, state.attacker);
  const bool hunter     = !state.hunter_declined &&
                      (effective_keywords(state, state.attacker) & HUNTER) != 0;

  auto choice             = Choice();
  choice.player_index     = hunter ? controller : 1 - controller;
  choice.description      = hunter ? "hunt" : "block";
  choice.text_description = hunter ? "Choose the blocker, or leave it to the "
                                     "opponent"
                                   : "Block the attack?";
  choice.actions          = [](Game& game) -> Choose {
    auto options = Choose_Card();
    for (int blocker : legal_blockers(static_cast<Game_State&>(game))) {
      options.targets.push_back(blocker);
    }
    // The last option: leave the decision to the defender when a hunter is
    // choosing, let the attack through when the defender is.
    options.targets.push_back(-1);
    options.up_to = true;
    return options;
  };
  choice.resolve = [hunter](Game& game, int index) -> Choice {
    Game_State& state    = static_cast<Game_State&>(game);
    auto        blockers = legal_blockers(state);
    if (index >= (int)blockers.size() && hunter) {
      state.hunter_declined = true;  // The defender is asked next.
      return null_choice;
    }
    state.blocker = index < (int)blockers.size() ? blockers[index] : -1;
    state.phase   = Phase::COMBAT;
    return null_choice;
  };
  return choice;
}

// Both creatures defeat each other when their power is equal, and a poisonous
// creature defeats whatever it fights. Which one is defeated is decided before
// either leaves play, since leaving changes the power of the others.
static void resolve_combat(Game_State& state) {
  const int attacker = state.attacker;
  if (state.blocker == -1) {
    lose_life(state, 1 - controller_of(state, attacker), 1);
  } else {
    const int  blocker        = state.blocker;
    const int  attacker_power = effective_power(state, attacker);
    const int  blocker_power  = effective_power(state, blocker);
    const bool blocker_defeated =
      attacker_power >= blocker_power ||
      (effective_keywords(state, attacker) & POISONOUS);
    const bool attacker_defeated =
      blocker_power >= attacker_power ||
      (effective_keywords(state, blocker) & POISONOUS);
    if (blocker_defeated) defeat_creature(state, blocker);
    if (attacker_defeated) defeat_creature(state, attacker);
  }
  state.blocker = -1;

  const bool may_attack_again = is_in_play(state, attacker) &&
                                state.attack_count < 2 &&
                                (effective_keywords(state, attacker) & FRENZY);
  state.phase = may_attack_again ? Phase::FRENZY : Phase::TURN_END;
}

void draw_back_up_to_hand_size(Game_State& state) {
  for (int i : {0, 1}) {
    Player& p = state.players[i];
    while (p.hand.size() < HAND_SIZE && p.draw_pile.size() > 0) {
      p.hand.push_back(p.draw_pile.back());
      p.draw_pile.pop_back();
    }
  }
}

static void end_turn(Game_State& state) {
  Player& player = state.active_player();
  draw_back_up_to_hand_size(state);
  state.attacker     = -1;
  state.blocker      = -1;
  state.attack_count = 0;
  if (state.extra_turn) {
    state.extra_turn = false;
  } else {
    state.current_player = 1 - state.current_player;
  }
  state.phase = Phase::TURN;
}

static bool has_targets(const Choose& choose) {
  return std::visit(
    [](const auto& option) { return option.targets.size() > 0; }, choose
  );
}

Choice Game_State::next_choice() {
  Game_State& state = *this;
  while (!state.game_over) {
    // Effects that owe a decision come first. One whose targets have all gone
    // in the meantime is dropped.
    if (!state.queue.empty()) {
      auto choice = state.queue.front();
      state.queue.erase(state.queue.begin());
      if (has_targets(choice.actions(state))) return choice;
      continue;
    }

    switch (state.phase) {
      case Phase::TURN:
        // No card to play and no creature to attack with: you lose.
        if (turn_actions(state).empty()) {
          end_game(state, 1 - state.current_player);
          continue;
        }
        return make_turn_choice(state);

      case Phase::MINDBUG:
        if (state.players[1 - state.current_player].mindbugs == 0) {
          resolve_played_creature(state, false);
          continue;
        }
        return make_mindbug_choice(state);

      case Phase::ATTACK:
        // A Defeated ability may have taken the attacker out between the two
        // attacks of a frenzy creature.
        if (!is_in_play(state, state.attacker)) {
          state.phase = Phase::TURN_END;
          continue;
        }
        state.attack_count += 1;
        state.hunter_declined = false;
        trigger_attack(state, state.attacker);
        state.phase = Phase::BLOCK;
        continue;

      case Phase::BLOCK:
        // An Attack ability may have taken the attacker out.
        if (!is_in_play(state, state.attacker)) {
          state.phase = Phase::TURN_END;
          continue;
        }
        if (legal_blockers(state).empty()) {
          state.blocker = -1;
          state.phase   = Phase::COMBAT;
          continue;
        }
        return make_block_choice(state);

      case Phase::COMBAT: resolve_combat(state); continue;

      case Phase::FRENZY: return make_frenzy_choice(state);

      case Phase::TURN_END: end_turn(state); continue;
    }
  }
  return null_choice;
}

// ---- Setup ----

void Game_State::init(int seed) {
  assert(!card_designs.empty() && "load_card_designs() has not been called");

  // A deal starts from an empty game, whatever was played here before.
  *this = Game_State();

  auto rng  = std::mt19937((unsigned int)seed);
  auto deck = full_deck_designs();
  std::shuffle(deck.begin(), deck.end(), rng);
  random_seed = (unsigned int)rng();

  // Only the cards dealt to the two players take part in the game.
  const int dealt_count = 2 * (HAND_SIZE + DRAW_PILE_SIZE);
  all_cards.assign(deck.begin(), deck.begin() + dealt_count);

  int card = 0;
  for (int player = 0; player < 2; ++player) {
    // The dealt 10 cards are split 5/5 at random. On the rules sheet the
    // player picks which 5 of the 10 go to hand.
    for (int i = 0; i < HAND_SIZE; ++i) {
      players[player].hand.push_back(card++);
    }
    for (int i = 0; i < DRAW_PILE_SIZE; ++i) {
      players[player].draw_pile.push_back(card++);
    }
  }
  begin_game();
}

Game_State quick_setup(int seed) {
  auto state = Game_State();
  state.init(seed);
  return state;
}

// }  // namespace mindbug
