#include "gameplay.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <random>

#include "models.h"

// ---- Local utilities ----

// Convert a list of Card_Id to packed-int targets for game's Choose_Card.
static std::vector<int> pack_targets(const std::vector<Card_Id>& cids) {
  std::vector<int> out;
  out.reserve(cids.size());
  for (const auto& c : cids) out.push_back(pack_card_id(c));
  return out;
}

// ---- Combinations ----

std::vector<std::vector<Card_Id>> all_combinations(
  const std::vector<Card_Id>& card_ids, int num_cards, bool up_to
) {
  int n = std::min(num_cards, (int)card_ids.size());
  std::vector<std::vector<Card_Id>> result;
  if (up_to) {
    // All subsets of size 0..n.
    for (int k = 0; k <= n; ++k) {
      if (k == 0) {
        result.push_back({});
        continue;
      }
      // Iterate combinations of card_ids choose k.
      auto              sz = (int)card_ids.size();
      std::vector<bool> mask(sz, false);
      std::fill(mask.end() - k, mask.end(), true);
      do {
        std::vector<Card_Id> combo;
        for (int i = 0; i < sz; ++i)
          if (mask[i]) combo.push_back(card_ids[i]);
        result.push_back(std::move(combo));
      } while (std::next_permutation(mask.begin(), mask.end()));
    }
  } else {
    auto sz = (int)card_ids.size();
    if (sz <= n) {
      result.push_back(card_ids);
      return result;
    }
    std::vector<bool> mask(sz, false);
    std::fill(mask.end() - n, mask.end(), true);
    do {
      std::vector<Card_Id> combo;
      for (int i = 0; i < sz; ++i)
        if (mask[i]) combo.push_back(card_ids[i]);
      result.push_back(std::move(combo));
    } while (std::next_permutation(mask.begin(), mask.end()));
  }
  return result;
}

// ---- Choice helpers ----

Choice make_choose_card_choice(
  int                                                      player_index,
  std::function<std::vector<Card_Id>(Game_State&)>         get_targets,
  std::function<std::vector<Choice>(Game_State&, Card_Id)> on_chosen,
  const char*                                              text_description
) {
  Choice c;
  c.player_index     = player_index;
  c.description      = "choose-card";
  c.text_description = text_description;
  c.actions          = [get_targets](Game& g) -> Choose {
    auto& gs = static_cast<Game_State&>(g);
    return Choose_Card{pack_targets(get_targets(gs)), true};
  };
  c.resolve = [get_targets, on_chosen](Game& g, int option_index) -> Choice {
    auto&   gs      = static_cast<Game_State&>(g);
    auto    targets = get_targets(gs);
    Card_Id chosen  = targets[option_index];
    if (Card_Id::is_null(chosen)) return null_choice;
    return queue_follow_ups(gs, on_chosen(gs, chosen));
  };
  return c;
}

Choice make_choose_cards_choice(
  int                                              player_index,
  std::function<std::vector<Card_Id>(Game_State&)> get_targets,
  std::function<int(Game_State&)>                  get_count,
  bool                                             up_to,
  std::function<std::vector<Choice>(Game_State&, std::vector<Card_Id>)>
              on_chosen,
  const char* text_description
) {
  Choice c;
  c.player_index     = player_index;
  c.description      = "choose-cards";
  c.text_description = text_description;
  c.actions          = [get_targets, get_count, up_to](Game& g) -> Choose {
    auto& gs = static_cast<Game_State&>(g);
    return Choose_Cards{pack_targets(get_targets(gs)), get_count(gs), up_to};
  };
  c.resolve = [get_targets,
               get_count,
               up_to,
               on_chosen](Game& g, int option_index) -> Choice {
    auto& gs     = static_cast<Game_State&>(g);
    auto  combos = all_combinations(get_targets(gs), get_count(gs), up_to);
    return queue_follow_ups(gs, on_chosen(gs, combos[option_index]));
  };
  return c;
}

// ---- Selection helpers ----

bool beats_opponent(
  Game_State&                                 game,
  int                                         player_index,
  const std::function<int(Game_State&, int)>& metric
) {
  int              n = (int)game.players.size();
  std::vector<int> scores(n);
  for (int i = 0; i < n; ++i) scores[i] = metric(game, i);
  return scores[player_index] > scores[1 - player_index];
}

std::vector<Card_Id> card_selection(
  Game_State&                             state,
  int                                     player_id,
  const std::string&                      area,
  const std::function<bool(const Card&)>& f,
  bool                                    include_null
) {
  std::vector<Card_Id> result;
  auto                 cards = state.card_list(player_id, area);
  for (const auto& cid : cards) {
    if (f(state.all_cards[cid.card_index])) result.push_back(cid);
  }
  std::sort(
    result.begin(), result.end(), [](const Card_Id& a, const Card_Id& b) {
      return a.card_index < b.card_index;
    }
  );
  if (include_null) result.push_back(Card_Id::null());
  return result;
}

std::vector<Card_Id> people_selection(
  Game_State& game, const std::function<bool(const Card&)>& f, bool include_null
) {
  std::vector<Card_Id> result;
  for (int pid : game.peoples) {
    if (f(game.all_cards[pid])) {
      result.push_back(Card_Id{"people", pid, game.owner(pid)});
    }
  }
  std::sort(
    result.begin(), result.end(), [](const Card_Id& a, const Card_Id& b) {
      return a.card_index < b.card_index;
    }
  );
  if (include_null) result.push_back(Card_Id::null());
  return result;
}

std::vector<Card_Id> wonders_selection(
  Game_State& game, const std::function<bool(const Card&)>& f
) {
  std::vector<Card_Id> result;
  for (int p = 0; p < (int)game.players.size(); ++p) {
    for (int wid : game.players[p].wonders) {
      if (f(game.all_cards[wid])) {
        result.push_back(Card_Id{"wonders", wid, p});
      }
    }
  }
  std::sort(
    result.begin(), result.end(), [](const Card_Id& a, const Card_Id& b) {
      return a.card_index < b.card_index;
    }
  );
  return result;
}

// ---- Game mechanics ----

std::vector<Choice> draw_card(
  Game_State& game, int player_id, bool replacement_effects
) {
  Player& player = game.players[player_id];
  if (player.deck.empty()) return {};

  if (replacement_effects) {
    for (int wid : player.wonders) {
      auto choices = game.all_cards[wid].on_draw_replacement(game);
      if (!choices.empty()) return choices;
    }
  }
  if (player.deck.empty()) return {};

  int card_id = player.deck.back();
  player.deck.pop_back();
  player.hand.push_back(card_id);

  for (int wid : player.wonders) {
    auto choices = game.all_cards[wid].on_draw(game);
    if (!choices.empty()) return choices;
  }
  return {};
}

std::vector<Choice> discard_cards(
  Game_State& game, const std::vector<Card_Id>& card_ids
) {
  if (card_ids.empty()) return {};
  // All cards must come from the same player's hand.
  for (const auto& cid : card_ids) {
    assert(cid.area == "hand");
    assert(cid.owner_index == card_ids[0].owner_index);
  }

  int                player_id = card_ids[0].owner_index;
  Player&            player    = game.players[player_id];
  std::vector<Card*> cards;
  for (const auto& cid : card_ids) {
    Card& card = game.all_cards[cid.card_index];
    auto  it   = std::find(player.hand.begin(), player.hand.end(), card.id);
    if (it != player.hand.end()) player.hand.erase(it);
    player.discard.push_back(card.id);
    cards.push_back(&card);
  }

  std::vector<Choice> choices;
  for (int wid : player.wonders) {
    for (Card* card : cards) {
      auto extra = game.all_cards[wid].on_discard(game, *card);
      for (auto& c : extra) choices.push_back(std::move(c));
    }
  }
  return choices;
}

std::vector<int> wonders_by_priority(Game_State& game) {
  const auto&      mine = game.active_player().wonders;
  std::vector<int> result(mine.begin(), mine.end());
  for (int wid : game.opponent().wonders) result.push_back(wid);
  return result;
}

std::vector<Choice> play_card(Game_State& game, const Card_Id& card_id) {
  Player& player = game.players[card_id.owner_index];
  Card&   card   = game.all_cards[card_id.card_index];

  if (card_id.area == "hand") {
    auto it =
      std::find(player.hand.begin(), player.hand.end(), card_id.card_index);
    if (it != player.hand.end()) player.hand.erase(it);
  }

  // Set before on_played runs. An effect looks up who owns the card it belongs
  // to, and every card type needs an answer there, not just wonders.
  card.owner = card_id.owner_index;

  std::vector<Choice> choices = game.all_cards[card.id].on_played(game);
  if (card.card_type == Card_Type::WONDER) {
    player.wonders.push_back(card.id);
  } else if (card.card_type == Card_Type::EVENT) {
    player.discard.push_back(card.id);
  } else if (card.card_type == Card_Type::PEOPLE) {
    game.peoples.push_back(card.id);
  }
  card.counters = 0;

  for (int wid : wonders_by_priority(game)) {
    game.all_cards[wid].on_play(game, card);
  }
  return choices;
}

void destroy_people(Game_State& game, const Card_Id& card_id) {
  Card& people = game.all_cards[card_id.card_index];
  for (int wid : wonders_by_priority(game)) {
    if (game.all_cards[wid].is_indestructible(game, people)) return;
  }
  people.destroyed = true;
  game.all_cards[people.id].on_destroyed(game);
  for (int wid : wonders_by_priority(game)) {
    game.all_cards[wid].on_destroy(game, people);
  }
}

void destroy_wonder(Game_State& game, const Card_Id& card_id) {
  Card& card = game.all_cards[card_id.card_index];
  assert(card.card_type == Card_Type::WONDER);
  int owner_idx = card_id.owner_index;
  if (owner_idx >= 0) {
    Player& player = game.players[owner_idx];
    auto it = std::find(player.wonders.begin(), player.wonders.end(), card.id);
    if (it != player.wonders.end()) player.wonders.erase(it);
    player.discard.push_back(card.id);
  }
  game.all_cards[card.id].on_destroyed(game);
  for (int wid : wonders_by_priority(game)) {
    game.all_cards[wid].on_destroy(game, card);
  }
}

void restore_people(Game_State& game, const Card_Id& card_id) {
  game.all_cards[card_id.card_index].destroyed = false;
}

void shuffle_card_into_deck(Game_State& game, const Card_Id& card_id) {
  Card& card = game.all_cards[card_id.card_index];
  assert(card.card_type == Card_Type::WONDER);
  assert(card_id.area == "wonders" || card_id.area == "discard");
  int owner_idx = card_id.owner_index;
  if (owner_idx >= 0) {
    Player& player = game.players[owner_idx];
    auto it = std::find(player.wonders.begin(), player.wonders.end(), card.id);
    if (it != player.wonders.end()) player.wonders.erase(it);
    card.counters = 0;
    player.deck.push_back(card.id);
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::shuffle(player.deck.begin(), player.deck.end(), rng);
  }
}

std::optional<Choice> make_claim_choice(Game_State& game) {
  int player_index   = game.current_player;
  int opponent_index = 1 - player_index;

  auto can_claim = [player_index](Game_State& g, int pid) -> bool {
    if (g.all_cards[pid].can_be_claimed(g, player_index)) return true;
    Card& people = g.all_cards[pid];
    for (int wid : g.players[player_index].wonders) {
      if (g.all_cards[wid].wins_tie(g, people)) return true;
    }
    return false;
  };

  auto build_actions = [opponent_index,
                        can_claim](Game_State& g) -> std::vector<Card_Id> {
    std::vector<Card_Id> result;
    for (int pid : g.peoples) {
      if (g.owner(pid) == opponent_index && can_claim(g, pid)) {
        result.push_back(Card_Id{"people", pid, opponent_index});
      }
    }
    result.push_back(Card_Id::null());
    return result;
  };

  if (build_actions(game).size() == 1) return std::nullopt;

  Choice c;
  c.player_index     = player_index;
  c.description      = "choose-card";
  c.text_description = "Claim a people card from your opponent";
  c.actions          = [build_actions](Game& g) -> Choose {
    auto& gs = static_cast<Game_State&>(g);
    return Choose_Card{pack_targets(build_actions(gs)), true};
  };
  c.resolve = [build_actions,
               player_index](Game& g, int option_index) -> Choice {
    auto&   gs      = static_cast<Game_State&>(g);
    auto    targets = build_actions(gs);
    Card_Id chosen  = targets[option_index];
    if (!Card_Id::is_null(chosen)) {
      Card& people = gs.get_card(chosen);
      people.owner = player_index;
    }
    return null_choice;
  };
  return c;
}

int compute_player_score(Game_State& game, int player_index) {
  int     score  = 0;
  Player& player = game.players[player_index];
  for (int people_id : game.peoples) {
    Card& people = game.all_cards[people_id];
    if (people.owner != player_index || people.destroyed) continue;
    int points = game.effective_power(people.id);
    for (int wid : player.wonders) {
      points = game.all_cards[wid].on_scoring_people(game, people, points);
    }
    score += points;
  }
  for (int wid : player.wonders) {
    score += game.all_cards[wid].on_scoring(game);
  }
  return score;
}

Choice make_main_choice(Game_State& game) {
  int player_index = game.current_player;

  auto build_actions = [](Game_State& g) -> std::vector<Card_Id> {
    std::vector<Card_Id> result;
    for (int cid : g.players[g.current_player].hand) {
      result.push_back(Card_Id{"hand", cid, g.current_player});
    }
    std::sort(
      result.begin(), result.end(), [](const Card_Id& a, const Card_Id& b) {
        return a.card_index < b.card_index;
      }
    );
    result.push_back(Card_Id::null());
    return result;
  };

  Choice c;
  c.player_index     = player_index;
  c.description      = "main";
  c.text_description = "Play a card or pass";
  c.actions          = [build_actions](Game& g) -> Choose {
    auto& gs = static_cast<Game_State&>(g);
    return Choose_Card{pack_targets(build_actions(gs)), true};
  };
  c.resolve = [build_actions](Game& g, int option_index) -> Choice {
    auto&   gs      = static_cast<Game_State&>(g);
    auto    actions = build_actions(gs);
    Card_Id chosen  = actions[option_index];
    if (!Card_Id::is_null(chosen)) {
      auto choices     = play_card(gs, chosen);
      gs.current_phase = Game_Phase::POST_PLAY;
      return queue_follow_ups(gs, choices);
    }
    std::vector<Choice> result;
    Player&             player = gs.active_player();
    for (int wid : player.wonders) {
      auto extra = gs.all_cards[wid].on_pass(gs);
      for (auto& ch : extra) result.push_back(std::move(ch));
    }
    gs.current_phase = Game_Phase::POST_PASS_EFFECTS;
    return queue_follow_ups(gs, result);
  };
  return c;
}

// ---- Bindings ----
