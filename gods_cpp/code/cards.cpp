#include "cards.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <algorithm>

#include "gameplay.h"
#include "models.h"

namespace nb = nanobind;
using namespace nb::literals;
using namespace std;

// Global registry definition (declared in models.h).
vector<unique_ptr<Card_Design>> card_designs;

// ---- Card hook delegations ----
// Card.* methods just look up the design by id and call into it. Defined here
// to break the include cycle between models.h and cards.h.

vector<Choice> Card::on_draw(Game_State& g) {
  return card_designs[id]->on_draw(g);
}
vector<Choice> Card::on_draw_replacement(Game_State& g) {
  return card_designs[id]->on_draw_replacement(g);
}
vector<Choice> Card::on_played(Game_State& g) {
  return card_designs[id]->on_played(g);
}
vector<Choice> Card::on_game_end(Game_State& g) {
  return card_designs[id]->on_game_end(g);
}
void Card::on_destroyed(Game_State& g) { card_designs[id]->on_destroyed(g); }
void Card::on_play(Game_State& g, Card& c) { card_designs[id]->on_play(g, c); }
void Card::on_destroy(Game_State& g, Card& c) {
  card_designs[id]->on_destroy(g, c);
}
void Card::on_restore(Game_State& g, Card& c) {
  card_designs[id]->on_restore(g, c);
}
vector<Choice> Card::on_discard(Game_State& g, Card& c) {
  return card_designs[id]->on_discard(g, c);
}
vector<Choice> Card::on_pass(Game_State& g) {
  return card_designs[id]->on_pass(g);
}
vector<Choice> Card::on_turn_end(Game_State& g) {
  return card_designs[id]->on_turn_end(g);
}
vector<Choice> Card::on_turn_start(Game_State& g) {
  return card_designs[id]->on_turn_start(g);
}
int Card::power_modifier(Game_State& g, const Card& c, int p) {
  return card_designs[id]->power_modifier(g, c, p);
}
bool Card::is_indestructible(Game_State& g, const Card& c) {
  return card_designs[id]->is_indestructible(g, c);
}
int Card::can_be_claimed(Game_State& g, int pi) {
  return card_designs[id]->can_be_claimed(g, pi);
}
int Card::on_scoring(Game_State& g) { return card_designs[id]->on_scoring(g); }
int Card::on_scoring_people(Game_State& g, const Card& p, int pts) {
  return card_designs[id]->on_scoring_people(g, p, pts);
}
bool Card::wins_tie(Game_State& g, const Card& p) {
  return card_designs[id]->wins_tie(g, p);
}

// ---- Card subclasses ----
// Each subclass mirrors the corresponding Python class in gods/cards.py.
// Hooks capture `int my_id = this->id` (a stable index into card_designs) so
// closures stay valid after Game_State is value-copied during minimax search.

namespace {

// Light: when game ends, may play a card with effective_power <= mine.
struct Light : Card_Design {
  vector<Choice> on_game_end(Game_State& game) override {
    int  my_id       = this->id;
    int  my_owner    = game.owner(my_id);
    auto get_targets = [my_id](Game_State& s) {
      return card_selection(
        s,
        s.owner(my_id),
        "hand",
        [my_id, &s](const Card& c) {
          return s.effective_power(c.id) <= s.effective_power(my_id);
        },
        true
      );
    };
    auto on_chosen = [](Game_State& s, Card_Id cid) {
      return play_card(s, cid);
    };
    return {make_choose_card_choice(
      my_owner, get_targets, on_chosen, "Play a card from your hand"
    )};
  }
};

// Moon: keep hand topped up to current power.
struct Moon : Card_Design {
  static vector<Choice> draw_back_up(Card_Design* self, Game_State& game) {
    int     my_id    = self->id;
    int     my_owner = game.owner(my_id);
    Player& player   = game.players[my_owner];
    if (player.deck.empty() ||
        (int)player.hand.size() >= game.effective_power(my_id))
      return {};
    return draw_card(game, my_owner);
  }
  vector<Choice> on_turn_start(Game_State& g) override {
    return draw_back_up(this, g);
  }
  vector<Choice> on_turn_end(Game_State& g) override {
    return draw_back_up(this, g);
  }
  vector<Choice> on_draw(Game_State& g) override {
    return draw_back_up(this, g);
  }
  void on_play(Game_State& g, Card&) override { draw_back_up(this, g); }
  vector<Choice> on_discard(Game_State& g, Card&) override {
    return draw_back_up(this, g);
  }
};

// War: when active player passes, may destroy an opposing people with power <=
// mine.
struct War : Card_Design {
  vector<Choice> on_pass(Game_State& game) override {
    int my_id    = this->id;
    int my_owner = game.owner(my_id);
    if (game.current_player != my_owner) return {};
    auto get_targets = [my_id](Game_State& s) {
      int power = s.effective_power(my_id);
      return people_selection(
        s,
        [&s, power](const Card& p) {
          return !p.destroyed && s.effective_power(p.id) <= power;
        },
        true
      );
    };
    auto on_chosen = [](Game_State& s, Card_Id cid) {
      destroy_people(s, cid);
      return vector<Choice>{};
    };
    return {make_choose_card_choice(
      game.current_player, get_targets, on_chosen, "Destroy a people"
    )};
  }
};

// Rivers: lets owner claim tied people with effective_power <= mine.
struct Rivers : Card_Design {
  bool wins_tie(Game_State& game, const Card& people) override {
    return game.effective_power(people.id) <= game.effective_power(this->id);
  }
};

// Earthquake: destroy all people with effective_power <= mine on play.
struct Earthquake : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  power   = game.effective_power(this->id);
    auto targets = people_selection(game, [&game, power](const Card& p) {
      return game.effective_power(p.id) <= power;
    });
    for (const auto& cid : targets) destroy_people(game, cid);
    return {};
  }
};

// Eruption: shuffle blue wonders back into their owner's deck.
struct Eruption : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id       = this->id;
    auto get_targets = [](Game_State& s) {
      return card_selection(s, -1, "wonders", [](const Card& c) {
        return c.color == Card_Color::BLUE;
      });
    };
    auto on_chosen = [](Game_State& s, vector<Card_Id> combo) {
      for (const auto& cid : combo) shuffle_card_into_deck(s, cid);
      return vector<Choice>{};
    };
    auto get_count = [my_id](Game_State& s) {
      return s.effective_power(my_id);
    };
    return {make_choose_cards_choice(
      game.current_player,
      get_targets,
      get_count,
      true,
      on_chosen,
      "Shuffle blue wonders back into decks"
    )};
  }
};

// Meteorite: destroy opponent people with effective_power <= mine on play.
struct Meteorite : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  power    = game.effective_power(this->id);
    int  opponent = 1 - game.current_player;
    auto targets =
      people_selection(game, [opponent, &game, power](const Card& p) {
        return p.owner == opponent && !p.destroyed &&
               game.effective_power(p.id) <= power;
      });
    for (const auto& cid : targets) destroy_people(game, cid);
    return {};
  }
};

// Miracle: play another event card from hand with extra power.
struct Miracle : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id       = this->id;
    auto get_targets = [](Game_State& s) {
      return card_selection(s, s.current_player, "hand", [](const Card& c) {
        return c.card_type == Card_Type::EVENT;
      });
    };
    auto on_chosen = [my_id](Game_State& s, Card_Id cid) {
      Card& card = s.get_card(cid);
      card.counters += s.effective_power(my_id);
      return play_card(s, cid);
    };
    return {make_choose_card_choice(
      game.current_player,
      get_targets,
      on_chosen,
      "Play a card with extra power"
    )};
  }
};

// Flashback: return event cards from discard back to hand (excluding self).
struct Flashback : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id       = this->id;
    int  my_owner    = game.owner(my_id);
    auto get_targets = [my_id, my_owner](Game_State& s) {
      return card_selection(s, my_owner, "discard", [my_id](const Card& c) {
        return c.card_type == Card_Type::EVENT && c.id != my_id;
      });
    };
    auto on_chosen = [my_owner](Game_State& s, vector<Card_Id> combo) {
      Player& player = s.players[my_owner];
      for (const auto& cid : combo) {
        Card& card = s.get_card(cid);
        auto  it = find(player.discard.begin(), player.discard.end(), card.id);
        if (it != player.discard.end()) player.discard.erase(it);
        player.hand.push_back(card.id);
      }
      return vector<Choice>{};
    };
    auto get_count = [my_id](Game_State& s) {
      return s.effective_power(my_id);
    };
    return {make_choose_cards_choice(
      my_owner,
      get_targets,
      get_count,
      false,
      on_chosen,
      "Return event cards from discard to hand"
    )};
  }
};

// Prophecy: play up to X extra cards (recursive).
struct Prophecy : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    return make_nth_choice(game, 0);
  }
  vector<Choice> make_nth_choice(Game_State& game, int n) {
    int my_id = this->id;
    int power = game.effective_power(my_id);
    if (n >= power) return {};
    int  my_owner    = game.owner(my_id);
    auto get_targets = [my_owner](Game_State& s) {
      return card_selection(
        s, my_owner, "hand", [](const Card&) { return true; }, true
      );
    };
    auto on_chosen = [my_id, n](Game_State& s, Card_Id cid) -> vector<Choice> {
      vector<Choice> result;
      if (!Card_Id::is_null(cid)) {
        auto play_choices = play_card(s, cid);
        for (auto& c : play_choices) result.push_back(std::move(c));
        // Look up design via the global registry so deepcopy-equivalents work.
        auto* design = static_cast<Prophecy*>(card_designs[my_id].get());
        auto  extra  = design->make_nth_choice(s, n + 1);
        for (auto& c : extra) result.push_back(std::move(c));
      }
      return result;
    };
    return {make_choose_card_choice(
      my_owner, get_targets, on_chosen, "Play an extra card"
    )};
  }
};

// Time_Warp: return wonders to hand.
struct Time_Warp : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id     = this->id;
    auto on_chosen = [](Game_State& s, vector<Card_Id> combo) {
      for (const auto& cid : combo) {
        Card&   card  = s.get_card(cid);
        Player& owner = s.players[card.owner];
        auto    it = find(owner.wonders.begin(), owner.wonders.end(), card.id);
        if (it != owner.wonders.end()) owner.wonders.erase(it);
        card.counters = 0;
        owner.hand.push_back(card.id);
      }
      return vector<Choice>{};
    };
    auto get_count = [my_id](Game_State& s) {
      return s.effective_power(my_id);
    };
    auto get_targets = [](Game_State& s) {
      return wonders_selection(s, [](const Card&) { return true; });
    };
    return {make_choose_cards_choice(
      game.current_player,
      get_targets,
      get_count,
      true,
      on_chosen,
      "Return wonders to hand"
    )};
  }
};

// Aurora: draw X cards.
struct Aurora : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int            power = game.effective_power(this->id);
    vector<Choice> result;
    for (int i = 0; i < power; ++i) {
      auto extra = draw_card(game, game.current_player);
      for (auto& c : extra) result.push_back(std::move(c));
    }
    return result;
  }
};

// Darkness: opponent discards X cards.
struct Darkness : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id       = this->id;
    int  my_owner    = game.owner(my_id);
    auto get_targets = [my_owner](Game_State& s) {
      return card_selection(s, 1 - my_owner, "hand");
    };
    auto on_chosen = [](Game_State& s, vector<Card_Id> combo) {
      return discard_cards(s, combo);
    };
    auto get_count = [my_id](Game_State& s) {
      return s.effective_power(my_id);
    };
    return {make_choose_cards_choice(
      1 - my_owner,
      get_targets,
      get_count,
      false,
      on_chosen,
      "Discard cards from opponent's hand"
    )};
  }
};

// Spring: add counters to a people.
struct Spring : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id     = this->id;
    auto on_chosen = [my_id](Game_State& s, Card_Id cid) {
      s.get_card(cid).counters += s.effective_power(my_id);
      return vector<Choice>{};
    };
    auto get_targets = [](Game_State& s) {
      return people_selection(s, [](const Card& p) { return !p.destroyed; });
    };
    return {make_choose_card_choice(
      game.current_player, get_targets, on_chosen, "Add counters to a people"
    )};
  }
};

// Regrowth: restore a destroyed people with effective_power <= mine.
struct Regrowth : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id       = this->id;
    auto get_targets = [my_id](Game_State& s) {
      int power = s.effective_power(my_id);
      return people_selection(s, [&s, power](const Card& p) {
        return p.destroyed && s.effective_power(p.id) <= power;
      });
    };
    auto on_chosen = [](Game_State& s, Card_Id cid) {
      s.get_card(cid).destroyed = false;
      return vector<Choice>{};
    };
    return {make_choose_card_choice(
      game.current_player, get_targets, on_chosen, "Restore a destroyed people"
    )};
  }
};

// Flood: subtract X from all people counters.
struct Flood : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int power = game.effective_power(this->id);
    for (int pid : game.peoples) game.all_cards[pid].counters -= power;
    return {};
  }
};

// Forgive: add counters to a people.
struct Forgive : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id     = this->id;
    auto on_chosen = [my_id](Game_State& s, Card_Id cid) {
      s.get_card(cid).counters += s.effective_power(my_id);
      return vector<Choice>{};
    };
    return {make_choose_card_choice(
      game.current_player,
      [](Game_State& s) { return people_selection(s); },
      on_chosen,
      "Add counters to a people"
    )};
  }
};

// Unmaking: destroy a wonder with effective_power <= mine.
struct Unmaking : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id       = this->id;
    auto get_targets = [my_id](Game_State& s) {
      int power = s.effective_power(my_id);
      return wonders_selection(s, [&s, power](const Card& w) {
        return s.effective_power(w.id) <= power;
      });
    };
    auto on_chosen = [](Game_State& s, Card_Id cid) {
      destroy_wonder(s, cid);
      return vector<Choice>{};
    };
    return {make_choose_card_choice(
      game.current_player, get_targets, on_chosen, "Destroy a wonder"
    )};
  }
};

// Revolt: destroy a people with effective_power <= mine.
struct Revolt : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id       = this->id;
    auto get_targets = [my_id](Game_State& s) {
      int power = s.effective_power(my_id);
      return people_selection(s, [&s, power](const Card& p) {
        return !p.destroyed && s.effective_power(p.id) <= power;
      });
    };
    auto on_chosen = [](Game_State& s, Card_Id cid) {
      destroy_people(s, cid);
      return vector<Choice>{};
    };
    return {make_choose_card_choice(
      game.current_player, get_targets, on_chosen, "Destroy a people"
    )};
  }
};

// Blessing: add counters to a wonder.
struct Blessing : Card_Design {
  vector<Choice> on_played(Game_State& game) override {
    int  my_id     = this->id;
    auto on_chosen = [my_id](Game_State& s, Card_Id cid) {
      s.get_card(cid).counters += s.effective_power(my_id);
      return vector<Choice>{};
    };
    return {make_choose_card_choice(
      game.current_player,
      [](Game_State& s) { return wonders_selection(s); },
      on_chosen,
      "Add counters to a wonder"
    )};
  }
};

// Wisdom: when active player passes, may play a non-people card with power <=
// mine.
struct Wisdom : Card_Design {
  vector<Choice> on_pass(Game_State& game) override {
    int my_id    = this->id;
    int my_owner = game.owner(my_id);
    if (game.current_player != my_owner) return {};
    auto get_targets = [my_id, my_owner](Game_State& s) {
      int power = s.effective_power(my_id);
      return card_selection(
        s,
        my_owner,
        "hand",
        [power](const Card& c) {
          return c.power <= power && c.card_type != Card_Type::PEOPLE;
        },
        true
      );
    };
    auto on_chosen = [](Game_State& s, Card_Id cid) {
      return play_card(s, cid);
    };
    return {make_choose_card_choice(
      my_owner, get_targets, on_chosen, "Play a card from your hand"
    )};
  }
};

// Knowledge: opponent events get -X power, min 1.
struct Knowledge : Card_Design {
  int power_modifier(Game_State& game, const Card& card, int power) override {
    int my_owner = game.owner(this->id);
    if (card.card_type == Card_Type::EVENT && card.owner == 1 - my_owner) {
      int reduction = game.effective_power(this->id);
      return max(1, power - reduction);
    }
    return power;
  }
};

// Sky: my other blue wonders get +X.
struct Sky : Card_Design {
  int power_modifier(Game_State& game, const Card& card, int power) override {
    int my_owner = game.owner(this->id);
    if (card.color == Card_Color::BLUE && card.id != this->id) {
      const auto& w = game.players[my_owner].wonders;
      if (find(w.begin(), w.end(), card.id) != w.end()) {
        return power + game.effective_power(this->id);
      }
    }
    return power;
  }
};

// Deserts: score destroyed people with power <= mine.
struct Deserts : Card_Design {
  int on_scoring_people(
    Game_State& game, const Card& people, int points
  ) override {
    int my_owner = game.owner(this->id);
    if (people.destroyed && people.owner == my_owner) {
      if (game.effective_power(people.id) <= game.effective_power(this->id)) {
        return card_designs[people.id]->can_be_claimed(game, my_owner);
      }
    }
    return points;
  }
};

// Forests: when active player passes, may restore a destroyed people with power
// <= mine.
struct Forests : Card_Design {
  vector<Choice> on_pass(Game_State& game) override {
    int my_id    = this->id;
    int my_owner = game.owner(my_id);
    if (game.current_player != my_owner) return {};
    auto get_targets = [my_id](Game_State& s) {
      int power = s.effective_power(my_id);
      return people_selection(
        s,
        [&s, power](const Card& p) {
          return p.destroyed && s.effective_power(p.id) <= power;
        },
        true
      );
    };
    auto on_chosen = [](Game_State& s, Card_Id cid) {
      s.get_card(cid).destroyed = false;
      return vector<Choice>{};
    };
    return {make_choose_card_choice(
      my_owner, get_targets, on_chosen, "Restore a destroyed people"
    )};
  }
};

// Mountains: my people with effective_power <= mine are indestructible.
struct Mountains : Card_Design {
  bool is_indestructible(Game_State& game, const Card& people) override {
    int my_owner = game.owner(this->id);
    if (people.owner == my_owner) {
      return game.effective_power(people.id) <= game.effective_power(this->id);
    }
    return false;
  }
};

// Animals: worth X points at end of game.
struct Animals : Card_Design {
  int on_scoring(Game_State& game) override {
    return game.effective_power(this->id);
  }
};

// Love: my alive people are worth +X points.
struct Love : Card_Design {
  int on_scoring_people(
    Game_State& game, const Card& people, int points
  ) override {
    if (people.owner == game.owner(this->id) && !people.destroyed) {
      return points + game.effective_power(this->id);
    }
    return points;
  }
};

// Seas: my alive people with effective_power <= mine are worth +1.
struct Seas : Card_Design {
  int on_scoring_people(
    Game_State& game, const Card& people, int points
  ) override {
    if (people.owner == game.owner(this->id) && !people.destroyed) {
      if (game.effective_power(people.id) <= game.effective_power(this->id))
        return points + 1;
    }
    return points;
  }
};

// Fire: my red events get +X.
struct Fire : Card_Design {
  int power_modifier(Game_State& game, const Card& card, int power) override {
    if (card.card_type == Card_Type::EVENT && card.color == Card_Color::RED &&
        card.owner == game.owner(this->id)) {
      return power + game.effective_power(this->id);
    }
    return power;
  }
};

// Sun: my green wonders (other than self) get +X.
struct Sun : Card_Design {
  int power_modifier(Game_State& game, const Card& card, int power) override {
    int my_owner = game.owner(this->id);
    if (card.color == Card_Color::GREEN &&
        card.card_type == Card_Type::WONDER && card.id != this->id) {
      const auto& w = game.players[my_owner].wonders;
      if (find(w.begin(), w.end(), card.id) != w.end()) {
        return power + game.effective_power(this->id);
      }
    }
    return power;
  }
};

// Stars: when drawing, may draw from shared deck instead.
struct Stars : Card_Design {
  vector<Choice> on_draw_replacement(Game_State& game) override {
    int my_id    = this->id;
    int my_owner = game.owner(my_id);
    if (game.current_player != my_owner) return {};
    if (game.shared_deck.empty()) return {};

    Choice c;
    c.player_index     = my_owner;
    c.description      = "choose-binary";
    c.text_description = "Choose how to draw a card";
    c.actions          = [](Game&) -> Choose {
      return Choose_Option{{"Draw from shared deck", "Draw normally"}};
    };
    c.resolve = [my_id, my_owner](Game& g, int option_index) -> vector<Choice> {
      auto& s = static_cast<Game_State&>(g);
      if (option_index == 0) {
        int     power  = s.effective_power(my_id);
        Player& player = s.players[my_owner];
        int     cid    = s.shared_deck.back();
        s.shared_deck.pop_back();
        Card& card = s.all_cards[cid];
        card.power = power;
        card.owner = my_owner;
        player.hand.push_back(cid);
        return {};
      }
      return draw_card(s, my_owner, false);
    };
    return {c};
  }
};

// People cards — define ownership criteria.

struct Egyptians : Card_Design {
  int can_be_claimed(Game_State& game, int player_index) override {
    auto metric = [](Game_State& g, int i) {
      int sum = 0;
      for (int wid : g.players[i].wonders) {
        if (g.all_cards[wid].color == Card_Color::GREEN)
          sum += g.effective_power(wid);
      }
      return sum;
    };
    return beats_opponent(game, player_index, metric);
  }
};

struct Greeks : Card_Design {
  int can_be_claimed(Game_State& game, int player_index) override {
    Player& p = game.players[player_index];
    Player& o = game.players[1 - player_index];
    return (int)(p.hand.size() >= 2 * o.hand.size() && o.hand.size() > 0);
  }
};

struct Vikings : Card_Design {
  int can_be_claimed(Game_State& game, int player_index) override {
    return beats_opponent(game, player_index, [](Game_State& g, int i) {
      return (int)g.players[i].deck.size();
    });
  }
};

struct Minoans : Card_Design {
  int can_be_claimed(Game_State& game, int player_index) override {
    return beats_opponent(game, player_index, [](Game_State& g, int i) {
      return (int)g.players[i].wonders.size();
    });
  }
};

struct Babylonians : Card_Design {
  int can_be_claimed(Game_State& game, int player_index) override {
    auto metric = [](Game_State& g, int i) {
      int sum = 0;
      for (int wid : g.players[i].wonders) sum += g.effective_power(wid);
      return sum;
    };
    return beats_opponent(game, player_index, metric);
  }
};

struct Romans : Card_Design {
  int can_be_claimed(Game_State& game, int player_index) override {
    auto metric = [](Game_State& g, int i) {
      int sum = 0;
      for (int wid : g.players[i].wonders) {
        if (g.all_cards[wid].color == Card_Color::RED)
          sum += g.effective_power(wid);
      }
      return sum;
    };
    return beats_opponent(game, player_index, metric);
  }
};

struct Judeans : Card_Design {
  int can_be_claimed(Game_State& game, int player_index) override {
    auto metric = [](Game_State& g, int i) {
      int sum = 0;
      for (int wid : g.players[i].wonders) {
        if (g.all_cards[wid].color == Card_Color::BLUE)
          sum += g.effective_power(wid);
      }
      return sum;
    };
    return beats_opponent(game, player_index, metric);
  }
};

// ---- Factory ----

template <typename T>
unique_ptr<Card_Design> make() {
  return make_unique<T>();
}

using Factory = unique_ptr<Card_Design> (*)();

const unordered_map<string, Factory>& card_classes() {
  static const unordered_map<string, Factory> classes = {
    {"Light", &make<Light>},
    {"Moon", &make<Moon>},
    {"War", &make<War>},
    {"Rivers", &make<Rivers>},
    {"Earthquake", &make<Earthquake>},
    {"Eruption", &make<Eruption>},
    {"Meteorite", &make<Meteorite>},
    {"Miracle", &make<Miracle>},
    {"Flashback", &make<Flashback>},
    {"Prophecy", &make<Prophecy>},
    {"Time Warp", &make<Time_Warp>},
    {"Aurora", &make<Aurora>},
    {"Darkness", &make<Darkness>},
    {"Spring", &make<Spring>},
    {"Regrowth", &make<Regrowth>},
    {"Flood", &make<Flood>},
    {"Forgive", &make<Forgive>},
    {"Unmaking", &make<Unmaking>},
    {"Revolt", &make<Revolt>},
    {"Blessing", &make<Blessing>},
    {"Wisdom", &make<Wisdom>},
    {"Knowledge", &make<Knowledge>},
    {"Sky", &make<Sky>},
    {"Deserts", &make<Deserts>},
    {"Forests", &make<Forests>},
    {"Mountains", &make<Mountains>},
    {"Animals", &make<Animals>},
    {"Love", &make<Love>},
    {"Seas", &make<Seas>},
    {"Fire", &make<Fire>},
    {"Sun", &make<Sun>},
    {"Stars", &make<Stars>},
    {"Egyptians", &make<Egyptians>},
    {"Greeks", &make<Greeks>},
    {"Vikings", &make<Vikings>},
    {"Minoans", &make<Minoans>},
    {"Babylonians", &make<Babylonians>},
    {"Romans", &make<Romans>},
    {"Judeans", &make<Judeans>},
  };
  return classes;
}

Card_Type type_from_string(const string& s) {
  if (s == "wonder") return Card_Type::WONDER;
  if (s == "event") return Card_Type::EVENT;
  if (s == "people") return Card_Type::PEOPLE;
  return Card_Type::EVENT;
}

Card_Color color_from_string(const string& s) {
  if (s == "green") return Card_Color::GREEN;
  if (s == "blue") return Card_Color::BLUE;
  if (s == "red") return Card_Color::RED;
  if (s == "yellow") return Card_Color::YELLOW;
  return Card_Color::RED;
}

}  // namespace

bool has_card_class(const string& name) {
  return card_classes().count(name) > 0;
}

unique_ptr<Card_Design> create_card_design(
  const string& name,
  const string& type_str,
  const string& color_str,
  const string& effect,
  int           id
) {
  unique_ptr<Card_Design> design;
  auto                    it = card_classes().find(name);
  if (it != card_classes().end()) {
    design = it->second();
  } else {
    design = make_unique<Card_Design>();
  }
  design->id        = id;
  design->name      = name;
  design->card_type = type_from_string(type_str);
  design->color     = color_from_string(color_str);
  design->effect    = effect;
  return design;
}

void bind_cards(nb::module_& m) {
  m.def("has_card_class", &has_card_class, "name"_a);
  // CARD_CLASSES is exposed as a Python dict[str, str] (name → class name).
  // Python rarely needs the actual class object — only Whether the name is
  // known.
  nb::dict classes;
  for (const auto& kv : card_classes()) classes[kv.first.c_str()] = kv.first;
  m.attr("CARD_CLASSES") = classes;
}
