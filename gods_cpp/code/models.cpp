#include "models.h"

#include <algorithm>
#include <numeric>
#include <sstream>

#include <game_cpp/game.h>

#include "gameplay.h"  // For make_main_choice / make_claim_choice in next_choice.

// ---- Card_Id packing ----

static int area_to_code(const std::string& area) {
  if (area == "deck")    return area_code::DECK;
  if (area == "hand")    return area_code::HAND;
  if (area == "discard") return area_code::DISCARD;
  if (area == "wonders") return area_code::WONDERS;
  if (area == "people")  return area_code::PEOPLE;
  return area_code::NONE;
}

static std::string code_to_area(int code) {
  switch (code) {
    case area_code::DECK:    return "deck";
    case area_code::HAND:    return "hand";
    case area_code::DISCARD: return "discard";
    case area_code::WONDERS: return "wonders";
    case area_code::PEOPLE:  return "people";
    default:                 return "none";
  }
}

int pack_card_id(const Card_Id& cid) {
  if (Card_Id::is_null(cid)) return 0;  // Null sentinel.
  int idx   = cid.card_index & 0xFFFFFF;
  int area  = (area_to_code(cid.area) & 0xF) << 24;
  int owner = ((cid.owner_index + 1) & 0xF) << 28;  // -1 -> 0, 0 -> 1, 1 -> 2.
  return idx | area | owner;
}

Card_Id unpack_card_id(int packed) {
  if (packed == 0) return Card_Id::null();
  Card_Id cid;
  cid.card_index  = packed & 0xFFFFFF;
  cid.area        = code_to_area((packed >> 24) & 0xF);
  cid.owner_index = ((packed >> 28) & 0xF) - 1;
  return cid;
}

// ---- Game_State methods ----

std::vector<Card_Id> Game_State::peoples_ids() const {
  std::vector<Card_Id> out;
  for (int pid : peoples) {
    out.push_back(Card_Id{"people", pid, all_cards[pid].owner});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

std::vector<Card_Id> Game_State::wonders_of(int player_index) const {
  std::vector<Card_Id> out;
  for (int wid : players[player_index].wonders) {
    out.push_back(Card_Id{"wonders", wid, player_index});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

std::vector<Card_Id> Game_State::discard_of(int player_index) const {
  std::vector<Card_Id> out;
  for (int did : players[player_index].discard) {
    out.push_back(Card_Id{"discard", did, player_index});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

std::vector<Card_Id> Game_State::hand_of(int player_index) const {
  std::vector<Card_Id> out;
  for (int hid : players[player_index].hand) {
    out.push_back(Card_Id{"hand", hid, player_index});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

std::vector<Card_Id> Game_State::card_list(int player_id, const std::string& area) const {
  if (player_id == -1) {
    auto a = card_list(0, area);
    auto b = card_list(1, area);
    a.insert(a.end(), b.begin(), b.end());
    std::sort(a.begin(), a.end(),
              [](const Card_Id& x, const Card_Id& y) { return x.card_index < y.card_index; });
    return a;
  }
  const Player& p = players[player_id];
  const std::vector<int>* src = nullptr;
  if      (area == "hand")    src = &p.hand;
  else if (area == "wonders") src = &p.wonders;
  else if (area == "discard") src = &p.discard;
  else if (area == "deck")    src = &p.deck;
  std::vector<Card_Id> out;
  if (src) {
    for (int cid : *src) out.push_back(Card_Id{area, cid, player_id});
  }
  std::sort(out.begin(), out.end(),
            [](const Card_Id& a, const Card_Id& b) { return a.card_index < b.card_index; });
  return out;
}

int Game_State::effective_power(int card_id) const {
  const Card& card = all_cards[card_id];
  int power = card.power + card.counters;
  for (const Player& player : players) {
    for (int wid : player.wonders) {
      // const_cast: power_modifier may be a virtual called on a non-const
      // Game_State&; semantically it must not mutate state during a query.
      power = const_cast<Card&>(all_cards[wid]).power_modifier(
        const_cast<Game_State&>(*this), card, power);
    }
  }
  if (power < 0) power = 0;
  return power;
}

std::optional<Choice> Game_State::next_choice() {
  while (!game_over) {
    if (!choices.empty()) {
      Choice c = std::move(choices.front());
      choices.erase(choices.begin());
      Choose actions = c.actions(*this);
      if (action_options_count(actions) == 0) continue;
      return c;
    }

    if (current_phase == "start") {
      for (int wid : active_player().wonders) {
        auto extra = all_cards[wid].on_turn_start(*this);
        for (auto& ch : extra) choices.push_back(std::move(ch));
      }
      current_phase = "main";
    } else if (current_phase == "main") {
      choices.push_back(make_main_choice(*this));
    } else if (current_phase == "post-play") {
      current_phase = "claim";
    } else if (current_phase == "post-pass-effects") {
      Player& player = active_player();
      if (player.deck.empty()) {
        game_over = true;
        continue;
      }
      auto extra = draw_card(*this, current_player);
      for (auto& ch : extra) choices.push_back(std::move(ch));
      current_phase = "post-pass-draw";
    } else if (current_phase == "post-pass-draw") {
      current_phase = "claim";
    } else if (current_phase == "claim") {
      auto claim = make_claim_choice(*this);
      if (claim) choices.push_back(std::move(*claim));
      current_phase = "end";
    } else if (current_phase == "end") {
      for (int wid : active_player().wonders) {
        auto extra = all_cards[wid].on_turn_end(*this);
        for (auto& ch : extra) choices.push_back(std::move(ch));
      }
      switch_turn();
      current_phase = "start";
    }
  }
  return std::nullopt;
}
