// Rules checks for Mindbug, plus a batch of random games.
//
// Run from the repository root, so that mindbug/cards.json is found:
//   compile mindbug mindbug_test

#include <game/agent.h>
#include <game/minimax.h>
#include <mindbug/cards.h>
#include <mindbug/gameplay.h>

#include <cmath>
#include <iostream>

using namespace mindbug;

static int failures = 0;

static void check(bool condition, const char* what) {
  if (condition) return;
  std::cerr << "FAILED: " << what << "\n";
  failures += 1;
}

// A creature in play, skipping the Play ability.
static int put(Game_State& state, int design, int controller) {
  state.all_cards.push_back(design);
  auto creature       = Creature();
  creature.card       = state.all_cards.size() - 1;
  creature.owner      = controller;
  creature.controller = controller;
  state.creatures.push_back(creature);
  return state.creatures.size() - 1;
}

// A card in a player's hand.
static int deal(Game_State& state, int design, int player) {
  state.all_cards.push_back(design);
  const int card = state.all_cards.size() - 1;
  state.players[player].hand.push_back(card);
  return card;
}

static void test_deck() {
  int total = 0;
  for (const Card_Design& design : card_designs) total += design.copies;
  check(card_designs.size() == DESIGN_COUNT, "32 card designs");
  check(total == 48, "48 cards in the deck");
  check(card_designs[GORILLION].power == 10, "Gorillion has power 10");
  check(
    (card_designs[PLATED_SCORPION].keywords & (TOUGH | POISONOUS)) ==
      (TOUGH | POISONOUS),
    "Plated Scorpion is tough and poisonous"
  );
}

static void test_power() {
  auto      state = Game_State();
  const int bugs  = put(state, SHIELD_BUGS, 0);
  const int owl   = put(state, SPIDER_OWL, 0);
  check(effective_power(state, bugs) == 4, "Shield Bugs does not boost itself");
  check(effective_power(state, owl) == 4, "Shield Bugs gives an ally +1");

  auto      werewolf_state = Game_State();
  const int werewolf       = put(werewolf_state, GOBLIN_WEREWOLF, 0);
  check(effective_power(werewolf_state, werewolf) == 8, "Werewolf on its turn");
  werewolf_state.current_player = 1;
  check(effective_power(werewolf_state, werewolf) == 2, "Werewolf off turn");

  auto      yeti_state = Game_State();
  const int yeti       = put(yeti_state, LONE_YETI, 0);
  check(effective_power(yeti_state, yeti) == 10, "Lone Yeti alone has +5");
  check(
    (effective_keywords(yeti_state, yeti) & FRENZY) != 0,
    "Lone Yeti alone has frenzy"
  );
  put(yeti_state, SPIDER_OWL, 0);
  check(effective_power(yeti_state, yeti) == 5, "Lone Yeti with company");
}

static void test_keywords() {
  auto      state   = Game_State();
  const int thrower = put(state, SNAIL_THROWER, 0);
  const int dog     = put(state, SHARK_DOG, 0);       // Power 4.
  const int bear    = put(state, BEE_BEAR, 0);        // Power 8.
  check(
    (effective_keywords(state, dog) & POISONOUS) != 0,
    "Snail Thrower arms a small ally"
  );
  check(
    (effective_keywords(state, bear) & POISONOUS) == 0,
    "Snail Thrower leaves a big ally alone"
  );
  check(
    (effective_keywords(state, thrower) & HUNTER) == 0,
    "Snail Thrower does not arm itself"
  );

  auto      sharky_state = Game_State();
  const int sharky       = put(sharky_state, SHARKY_CRAB_DOG_MUMMYPUS, 0);
  check(
    effective_keywords(sharky_state, sharky) == 0, "Sharky alone has nothing"
  );
  put(sharky_state, SPIDER_OWL, 1);  // Sneaky, poisonous.
  check(
    (effective_keywords(sharky_state, sharky) & (SNEAKY | POISONOUS)) ==
      (SNEAKY | POISONOUS),
    "Sharky copies the enemy keywords"
  );
}

static void test_blocking() {
  auto      state    = Game_State();
  const int sniper   = put(state, CHAMELEON_SNIPER, 0);  // Sneaky.
  const int owl      = put(state, SPIDER_OWL, 1);        // Sneaky.
  const int gorillion = put(state, GORILLION, 1);
  check(can_block(state, sniper, owl), "sneaky blocks sneaky");
  check(!can_block(state, sniper, gorillion), "sneaky is not blocked by others");

  const int bear = put(state, BEE_BEAR, 0);
  check(can_block(state, bear, gorillion), "Bee Bear is blocked by power 10");
  check(!can_block(state, bear, owl), "Bee Bear is not blocked by power 3");
}

static void test_tough() {
  auto      state  = Game_State();
  const int turtle = put(state, RHINO_TURTLE, 0);
  defeat_creature(state, turtle);
  check(state.creatures[turtle].alive, "tough survives the first defeat");
  check(state.creatures[turtle].exhausted, "tough is exhausted instead");
  defeat_creature(state, turtle);
  check(!state.creatures[turtle].alive, "tough dies the second time");
  check(
    state.players[0].discard.size() == 1, "a defeated creature is discarded"
  );
}

// The rule the game is named after: the opponent may take the creature you
// play, which costs them a Mindbug and gives you another turn.
static void test_mindbug_steal() {
  auto state = Game_State();
  deal(state, GORILLION, 0);
  deal(state, SPIDER_OWL, 0);
  deal(state, KILLER_BEE, 1);
  state.begin_game();

  resolve_choice(state, 0);  // Player 0 plays Gorillion.
  check(pending_choice(state).description == "mindbug", "the Mindbug is offered");
  check(pending_choice(state).player_index == 1, "offered to the opponent");

  resolve_choice(state, 0);  // Player 1 uses a Mindbug.
  check(state.creatures.size() == 1, "the creature is in play");
  check(state.creatures[0].controller == 1, "it changed sides");
  check(state.creatures[0].owner == 0, "it kept its owner");
  check(state.players[1].mindbugs == 1, "a Mindbug is spent");
  check(pending_choice(state).player_index == 0, "the player plays again");
}

// A hunter's controller picks the blocker, but may hand that back instead of
// forcing the attack through unblocked.
static void test_hunter_declines() {
  auto state = Game_State();
  put(state, KILLER_BEE, 0);   // Hunter.
  put(state, GORILLION, 1);    // The only creature that could block.
  deal(state, KILLER_BEE, 1);  // So player 1 still has a turn to take.
  state.begin_game();

  resolve_choice(state, 0);  // Player 0 attacks with its only creature.
  check(pending_choice(state).description == "hunt", "the hunter chooses");
  check(pending_choice(state).player_index == 0, "and it is its controller");

  // The last option leaves the choice to the defender.
  resolve_choice(state, pending_action_count(state) - 1);
  check(pending_choice(state).description == "block", "the defender chooses");
  check(pending_choice(state).player_index == 1, "and it is the defender");

  resolve_choice(state, pending_action_count(state) - 1);  // Don't block.
  check(state.players[1].life == STARTING_LIFE - 1, "an unblocked attack hits");
}

// A sampled deal keeps everything the player has seen and stays a deal the
// 48-card deck could have produced.
static void test_sampling() {
  Game_State   state = quick_setup(11);
  std::mt19937 rng(11);

  // Move the game along so there is something in play and in a discard pile.
  Agent_Random agent(11);
  for (int i = 0; i < 30 && !state.is_game_over(); ++i) {
    resolve_choice(state, agent.choose_action(state, pending_choice(state)));
  }

  for (int player = 0; player < 2; ++player) {
    Game_State sampled = sample_state(state, player, rng);

    for (int card : state.players[player].hand) {
      check(
        design_of(sampled, card) == design_of(state, card), "my hand is kept"
      );
    }
    for (int i = 0; i < state.creatures.size(); ++i) {
      check(
        creature_design(sampled, i) == creature_design(state, i),
        "creatures are kept"
      );
    }
    for (int seat = 0; seat < 2; ++seat) {
      for (int card : state.players[seat].discard) {
        check(
          design_of(sampled, card) == design_of(state, card),
          "discard piles are kept"
        );
      }
      check(
        sampled.players[seat].hand.size() ==
          state.players[seat].hand.size(),
        "hand sizes are kept"
      );
      check(
        sampled.players[seat].draw_pile.size() ==
          state.players[seat].draw_pile.size(),
        "draw pile sizes are kept"
      );
    }

    // No design turns up more often than the deck prints it.
    std::vector<int> dealt(DESIGN_COUNT, 0);
    for (int card = 0; card < sampled.all_cards.size(); ++card) {
      dealt[design_of(sampled, card)] += 1;
    }
    for (int design = 0; design < DESIGN_COUNT; ++design) {
      check(
        dealt[design] <= card_designs[design].copies,
        "a sampled deal fits in the deck"
      );
    }
  }
}

static void test_random_games() {
  const int num_games = 200;
  for (int game_index = 0; game_index < num_games; ++game_index) {
    Game_State   state = quick_setup(game_index);
    Agent_Random agent(game_index);
    int          decisions = 0;
    while (!state.is_game_over() && decisions < 2000) {
      resolve_choice(state, agent.choose_action(state, pending_choice(state)));
      decisions += 1;
      check(
        std::isfinite(evaluate_state(state, 0)), "the evaluation is a number"
      );
    }
    check(state.is_game_over(), "a random game ends");
    check(state.winner == 0 || state.winner == 1, "a random game has a winner");
    check(
      compute_player_score(state, state.winner) == 1, "the winner scores 1"
    );
  }
}

// A searching agent has to beat a random one clearly, or the state evaluation
// is pointing the wrong way.
static void test_search_agent() {
  const int                                num_games = 10;
  Agent_Minimax_Stochastic<Game_State>     searching(3, 8);
  Agent_Random                             random_agent(7);
  int                                      search_wins = 0;
  for (int game_index = 0; game_index < num_games; ++game_index) {
    // Alternate seats so neither agent benefits from leading.
    const bool search_is_player_0 = game_index % 2 == 0;
    Agent_Duel duel(&searching, &random_agent, !search_is_player_0);
    Game_State state = quick_setup(1000 + game_index);
    game_loop(state, duel);
    search_wins +=
      compute_player_score(state, search_is_player_0 ? 0 : 1);
  }
  std::cout << "minimax won " << search_wins << "/" << num_games
            << " against random\n";
  check(search_wins * 2 > num_games, "the searching agent beats random");
}

int main() {
  if (!load_card_designs()) {
    std::cerr << "run this from the repository root\n";
    return 1;
  }
  test_deck();
  test_power();
  test_keywords();
  test_blocking();
  test_tough();
  test_mindbug_steal();
  test_hunter_declines();
  test_sampling();
  test_random_games();
  test_search_agent();

  if (failures > 0) {
    std::cerr << failures << " checks failed\n";
    return 1;
  }
  std::cout << "all checks passed\n";
  return 0;
}
