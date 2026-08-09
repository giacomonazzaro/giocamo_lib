#include "gameplay.h"

#include <cassert>

#include "cards.h"

namespace mindbug {

// ---- Queries ----

std::vector<int> creatures_of(const Game_State& state, int player) {
  std::vector<int> result;
  for (int i = 0; i < state.creatures.size(); ++i) {
    if (state.creatures[i].alive && state.creatures[i].controller == player) {
      result.push_back(i);
    }
  }
  return result;
}

int effective_power(const Game_State& state, int creature_index) {
  const Creature& creature = state.creatures[creature_index];
  const bool      its_turn = state.current_player == creature.controller;
  const int       design   = creature_design(state, creature_index);
  int             power    = card_designs[design].power;
  int             allies   = 0;
  for (int i = 0; i < state.creatures.size(); ++i) {
    const Creature& other = state.creatures[i];
    if (!other.alive || other.controller != creature.controller) continue;
    allies += 1;
    if (i == creature_index) continue;
    const int other_design = creature_design(state, i);
    if (other_design == SHIELD_BUGS) power += 1;
    if (other_design == URCHIN_HURLER && its_turn) power += 2;
  }
  if (design == GOBLIN_WEREWOLF && its_turn) power += 6;
  if (design == LONE_YETI && allies == 1) power += 5;
  return power;
}

// mirror=false stops Sharky from copying another Sharky's copied keywords.
static int keywords_of(
  const Game_State& state, int creature_index, bool mirror
) {
  const Creature& creature = state.creatures[creature_index];
  const int       design   = creature_design(state, creature_index);
  int             keywords = card_designs[design].keywords;
  std::vector<int> allies  = creatures_of(state, creature.controller);

  if (design == LONE_YETI && allies.size() == 1) keywords |= FRENZY;
  if (effective_power(state, creature_index) <= 4) {
    for (int ally : allies) {
      if (ally != creature_index &&
          creature_design(state, ally) == SNAIL_THROWER) {
        keywords |= HUNTER | POISONOUS;
      }
    }
  }
  if (design == SHARKY_CRAB_DOG_MUMMYPUS && mirror) {
    for (int enemy : creatures_of(state, 1 - creature.controller)) {
      keywords |= keywords_of(state, enemy, false) &
                  (HUNTER | SNEAKY | FRENZY | POISONOUS);
    }
  }
  return keywords;
}

int effective_keywords(const Game_State& state, int creature_index) {
  return keywords_of(state, creature_index, true);
}

std::vector<int> creature_targets(
  const Game_State& state, int controller, int min_power, int max_power
) {
  std::vector<int> targets;
  for (int i = 0; i < state.creatures.size(); ++i) {
    if (!state.creatures[i].alive) continue;
    if (controller != -1 && state.creatures[i].controller != controller)
      continue;
    const int power = effective_power(state, i);
    if (power < min_power || power > max_power) continue;
    targets.push_back(i);
  }
  return targets;
}

bool can_block(const Game_State& state, int attacker, int blocker) {
  if (effective_keywords(state, attacker) & SNEAKY) {
    if (!(effective_keywords(state, blocker) & SNEAKY)) return false;
  }
  const int power           = effective_power(state, blocker);
  const int attacker_design = creature_design(state, attacker);
  if (attacker_design == BEE_BEAR && power <= 6) return false;
  if (attacker_design == ELEPHANTOPUS && power <= 4) return false;
  return true;
}

std::vector<Turn_Action> turn_actions(const Game_State& state) {
  std::vector<Turn_Action> actions;
  for (int i = 0; i < state.players[state.current_player].hand.size(); ++i) {
    actions.push_back(Turn_Action{false, i});
  }
  for (int i : creatures_of(state, state.current_player)) {
    actions.push_back(Turn_Action{true, i});
  }
  return actions;
}

int compute_player_score(const Game_State& state, int player) {
  return state.winner == player ? 1 : 0;
}

// ---- Choice helpers ----

std::vector<std::vector<int>> target_combinations(
  const std::vector<int>& targets, int count, bool up_to
) {
  const int                     size = (int)targets.size();
  std::vector<std::vector<int>> result;
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
    std::vector<bool> mask(size, false);
    std::fill(mask.end() - k, mask.end(), true);
    do {
      std::vector<int> selection;
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

void lose_life(Game_State& state, int player, int amount) {
  state.players[player].life -= amount;
  if (state.players[player].life <= 0) end_game(state, 1 - player);
}

int enter_play(Game_State& state, int card, int owner, int controller) {
  auto creature       = Creature();
  creature.card       = card;
  creature.owner      = owner;
  creature.controller = controller;
  state.creatures.push_back(creature);
  const int index = state.creatures.size() - 1;

  // A Deathweaver on the other side switches the Play ability off.
  for (int enemy : creatures_of(state, 1 - controller)) {
    if (creature_design(state, enemy) == DEATHWEAVER) return index;
  }
  trigger_play(state, index);
  return index;
}

void defeat_creature(Game_State& state, int creature_index) {
  Creature& creature = state.creatures[creature_index];
  if (!creature.alive) return;
  if ((effective_keywords(state, creature_index) & TOUGH) &&
      !creature.exhausted) {
    creature.exhausted = true;
    return;
  }
  creature.alive = false;
  state.players[creature.owner].discard.push_back(creature.card);
  trigger_defeated(state, creature_index);
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
      state.attacker     = action.index;
      state.attack_count = 0;
      state.phase        = Phase::ATTACK;
    } else {
      Player& player      = state.active_player();
      state.played_card = player.hand[action.index];
      player.hand.erase(player.hand.begin() + action.index);
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
  const int owner   = state.current_player;
  const int thief   = 1 - owner;
  const int card    = state.played_card;
  state.played_card = -1;
  state.phase       = Phase::TURN_END;
  if (stolen) {
    state.players[thief].mindbugs -= 1;
    state.extra_turn = true;
  }
  enter_play(state, card, owner, stolen ? thief : owner);
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

static std::vector<int> legal_blockers(const Game_State& state) {
  const int        defender = 1 - state.creatures[state.attacker].controller;
  std::vector<int> blockers;
  for (int candidate : creatures_of(state, defender)) {
    if (can_block(state, state.attacker, candidate)) {
      blockers.push_back(candidate);
    }
  }
  return blockers;
}

static Choice make_block_choice(Game_State& state) {
  // A hunter's controller picks the blocker, unless they pass the decision
  // back; then the defender chooses as usual.
  const int  controller = state.creatures[state.attacker].controller;
  const bool hunter =
    !state.hunter_declined &&
    (effective_keywords(state, state.attacker) & HUNTER) != 0;

  auto choice             = Choice();
  choice.player_index     = hunter ? controller : 1 - controller;
  choice.description      = hunter ? "hunt" : "block";
  choice.text_description = hunter ? "Choose the blocker, or leave it to the "
                                     "opponent"
                                   : "Block the attack?";
  choice.actions = [](Game& game) -> Choose {
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
    Game_State&      state    = static_cast<Game_State&>(game);
    std::vector<int> blockers = legal_blockers(state);
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
    lose_life(state, 1 - state.creatures[attacker].controller, 1);
  } else {
    const int  blocker         = state.blocker;
    const int  attacker_power  = effective_power(state, attacker);
    const int  blocker_power   = effective_power(state, blocker);
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

  const bool attacks_again = state.creatures[attacker].alive &&
                             state.attack_count < 2 &&
                             (effective_keywords(state, attacker) & FRENZY);
  state.phase = attacks_again ? Phase::ATTACK : Phase::TURN_END;
}

static void end_turn(Game_State& state) {
  Player& player = state.active_player();
  while (player.hand.size() < HAND_SIZE && player.draw_pile.size() > 0) {
    player.hand.push_back(player.draw_pile.back());
    player.draw_pile.pop_back();
  }
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
      Choice choice = state.queue.front();
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
        state.attack_count += 1;
        state.hunter_declined = false;
        trigger_attack(state, state.attacker);
        state.phase = Phase::BLOCK;
        continue;

      case Phase::BLOCK:
        // An Attack ability may have taken the attacker out.
        if (!state.creatures[state.attacker].alive) {
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

      case Phase::TURN_END: end_turn(state); continue;
    }
  }
  return null_choice;
}

// ---- Setup ----

Game_State quick_setup(int seed) {
  assert(!card_designs.empty() && "load_card_designs() has not been called");

  std::vector<int> deck;
  for (int design = 0; design < (int)card_designs.size(); ++design) {
    for (int copy = 0; copy < card_designs[design].copies; ++copy) {
      deck.push_back(design);
    }
  }
  auto rng = std::mt19937((unsigned int)seed);
  std::shuffle(deck.begin(), deck.end(), rng);

  auto state        = Game_State();
  state.random_seed = (unsigned int)seed + 1;

  // Only the cards dealt to the two players take part in the game.
  const int dealt_count = 2 * (HAND_SIZE + DRAW_PILE_SIZE);
  state.all_cards.assign(deck.begin(), deck.begin() + dealt_count);

  int card = 0;
  for (int player = 0; player < 2; ++player) {
    // The dealt 10 cards are split 5/5 at random. On the rules sheet the
    // player picks which 5 of the 10 go to hand.
    for (int i = 0; i < HAND_SIZE; ++i) {
      state.players[player].hand.push_back(card++);
    }
    for (int i = 0; i < DRAW_PILE_SIZE; ++i) {
      state.players[player].draw_pile.push_back(card++);
    }
  }
  state.begin_game();
  return state;
}

}  // namespace mindbug
