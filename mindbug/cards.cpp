#include "cards.h"

#include <mindbug/gameplay.h>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace mindbug {

std::vector<Card_Design> card_designs;

static int parse_keyword(const std::string& name) {
  if (name == "sneaky") return SNEAKY;
  if (name == "hunter") return HUNTER;
  if (name == "frenzy") return FRENZY;
  if (name == "poisonous") return POISONOUS;
  if (name == "tough") return TOUGH;
  std::cerr << "mindbug: unknown keyword " << name << "\n";
  return 0;
}

bool load_card_designs(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "mindbug: could not open " << path << "\n";
    return false;
  }
  nlohmann::json data;
  try {
    file >> data;
  } catch (const std::exception& error) {
    std::cerr << "mindbug: could not parse " << path << ": " << error.what()
              << "\n";
    return false;
  }
  if (!data.is_array() || data.size() != DESIGN_COUNT) {
    std::cerr << "mindbug: " << path << " must hold " << (int)DESIGN_COUNT
              << " cards\n";
    return false;
  }

  card_designs.clear();
  for (const auto& entry : data) {
    auto design     = Card_Design();
    design.name     = entry.value("name", std::string());
    design.text     = entry.value("text", std::string());
    design.image    = entry.value("image", std::string());
    design.power    = entry.value("power", 0);
    design.copies   = entry.value("copies", 1);
    design.keywords = 0;
    for (const auto& keyword : entry["keywords"]) {
      design.keywords |= parse_keyword(keyword.get<std::string>());
    }
    card_designs.push_back(design);
  }
  return true;
}

// ---- Effect helpers ----

// Every card of a pile, as choice targets.
template <int N>
static std::vector<int> cards_of(const Array_Inline<int, N>& pile) {
  return std::vector<int>(pile.begin(), pile.end());
}

static void discard_from_hand(
  Game_State& state, int player, const std::vector<int>& cards
) {
  for (int card : cards) {
    remove_card(state.players[player].hand, card);
    state.players[player].discard.push_back(card);
  }
  draw_back_up_to_hand_size(state);
}

// Play `card` out of `pile_owner`'s discard pile, under the control of
// `controller`.
static void play_from_discard(
  Game_State& state, int pile_owner, int controller, int card
) {
  remove_card(state.players[pile_owner].discard, card);
  enter_play(state, card, controller);
}

// A number in [0, bound). Only Strange Barrel needs this.
static int next_random(Game_State& state, int bound) {
  state.random_seed = state.random_seed * 1664525u + 1013904223u;
  return (int)((state.random_seed >> 16) % (unsigned int)bound);
}

// ---- Abilities ----

void trigger_play(Game_State& state, int card) {
  const int design = design_of(state, card);
  const int me     = controller_of(state, card);
  const int them   = 1 - me;

  switch (design) {
    case AXOLOTL_HEALER: state.players[me].life += 2; break;

    case BRAIN_FLY:
      state.queue.push_back(make_choice(
        me,
        "take-control",
        [](Game_State& game) { return creature_targets(game, -1, 6, 99); },
        [me](Game_State& game, int target) { take_control(game, target, me); }
      ));
      break;

    case COMPOST_DRAGON:
      state.queue.push_back(make_choice(
        me,
        "play-from-discard",
        [me](Game_State& game) { return cards_of(game.players[me].discard); },
        [me](Game_State& game, int card) {
          play_from_discard(game, me, me, card);
        }
      ));
      break;

    case FERRET_BOMBER:
      state.queue.push_back(make_multi_choice(
        them,
        "discard",
        [them](Game_State& game) { return cards_of(game.players[them].hand); },
        2,
        false,
        [them](Game_State& game, const std::vector<int>& cards) {
          discard_from_hand(game, them, cards);
        }
      ));
      break;

    case GIRAFFODILE: {
      Player& player = state.players[me];
      player.hand.append(player.discard.begin(), player.discard.end());
      player.discard.clear();
      break;
    }

    case GRAVE_ROBBER:
      state.queue.push_back(make_choice(
        me,
        "play-from-discard",
        [them](Game_State& game) {
          return cards_of(game.players[them].discard);
        },
        [me, them](Game_State& game, int card) {
          play_from_discard(game, them, me, card);
        }
      ));
      break;

    case KANGASAURUS_REX: {
      // Snapshot first: defeating one creature can move the others around.
      std::vector<int> victims = creature_targets(state, them, 0, 4);
      for (int victim : victims) defeat_creature(state, victim);
      break;
    }

    case KILLER_BEE: lose_life(state, them, 1); break;

    case MYSTERIOUS_MERMAID:
      state.players[me].life = state.players[them].life;
      break;

    case TIGER_SQUIRREL:
      state.queue.push_back(make_choice(
        me,
        "defeat",
        [them](Game_State& game) {
          return creature_targets(game, them, 7, 99);
        },
        [](Game_State& game, int target) { defeat_creature(game, target); }
      ));
      break;

    default: break;
  }
}

void trigger_attack(Game_State& state, int card) {
  const int design = design_of(state, card);
  const int me     = controller_of(state, card);
  const int them   = 1 - me;

  switch (design) {
    case CHAMELEON_SNIPER: lose_life(state, them, 1); break;

    case SHARK_DOG:
      state.queue.push_back(make_choice(
        me,
        "defeat",
        [them](Game_State& game) {
          return creature_targets(game, them, 6, 99);
        },
        [](Game_State& game, int target) { defeat_creature(game, target); }
      ));
      break;

    case SNAIL_HYDRA:
      if (creature_targets(state, me, 0, 99).size() <
          creature_targets(state, them, 0, 99).size()) {
        state.queue.push_back(make_choice(
          me,
          "defeat",
          [](Game_State& game) { return creature_targets(game, -1, 0, 99); },
          [](Game_State& game, int target) { defeat_creature(game, target); }
        ));
      }
      break;

    case TURBO_BUG:
      if (state.players[them].life > 1) state.players[them].life = 1;
      break;

    case TUSKED_EXTORTER:
      state.queue.push_back(make_choice(
        them,
        "discard",
        [them](Game_State& game) { return cards_of(game.players[them].hand); },
        [them](Game_State& game, int card) {
          discard_from_hand(game, them, {card});
        }
      ));
      break;

    default: break;
  }
}

void trigger_defeated(Game_State& state, int card, int controller) {
  const int design = design_of(state, card);
  const int me     = controller;
  const int them   = 1 - me;

  switch (design) {
    case EXPLOSIVE_TOAD:
      state.queue.push_back(make_choice(
        me,
        "defeat",
        [](Game_State& game) { return creature_targets(game, -1, 0, 99); },
        [](Game_State& game, int target) { defeat_creature(game, target); }
      ));
      break;

    case HARPY_MOTHER:
      state.queue.push_back(make_multi_choice(
        me,
        "take-control",
        [](Game_State& game) { return creature_targets(game, -1, 0, 5); },
        2,
        true,
        [me](Game_State& game, const std::vector<int>& targets) {
          for (int target : targets) take_control(game, target, me);
        }
      ));
      break;

    case STRANGE_BARREL: {
      for (int i = 0; i < 2; ++i) {
        Player& victim = state.players[them];
        if (victim.hand.size() == 0) break;
        const int position = next_random(state, victim.hand.size());
        state.players[me].hand.push_back(victim.hand[position]);
        victim.hand.erase(victim.hand.begin() + position);
      }
      break;
    }

    default: break;
  }
}

}  // namespace mindbug
