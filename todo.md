- Update all games implemetation to new Giocamo API
- Simplfy Giocamo (play.cpp)
- move card implementation to mindbug/cards.cpp
- Adaptive resolution
- Finish wrapper for setting thing.children so animation are always handled.
- power as sub-thing


Major bugs
1. Every card effect in the running app is dead. main.cpp:62 passes std::to_string(i) — the array index — as the card's name when loading cards.json, throwing away the real "name" field. cards.cpp:852 looks the design up in a registry keyed by real names ("Miracle", "Flashback", …), finds nothing, and falls back to the base Card_Design, whose hooks all do nothing. All 864 lines of cards.cpp are unreachable in the app. I confirmed this: with the index as the name, a random playthrough produces only main choices; passing the real name, choose-card and choose-cards choices start appearing. has_card_class() is never called by anyone, which is probably why nobody noticed.

2. The search plays a different game than the game loop does. There are two incompatible ways to advance a game and both are live. The game loop uses the Choice that resolve returns (game.cpp:72). Minimax and MCTS throw that away and call state.next_choice() instead (minimax.h:88, mcts.h:103). For Gods, next_choice() pops the pending-choice queue (models.cpp:141), and resolve already popped one via resume() — so the search silently skips every choice that a card effect produces. Running both protocols on the same seed with the same agent: the game loop sees main, choose-cards, main while the search sees main, main. Connect Four and Dot happen to be safe because their next_choice() is a pure read of the board; Tressette is safe by accident.

3. Event cards never get an owner, so a Choice can carry player_index = -1. gameplay.cpp:264-269 sets card.owner only for wonders. Events go straight to the discard with owner still at its default of -1. Flashback and Prophecy then do int my_owner = game.owner(my_id) and build a choice with player_index = my_owner. That reaches Agent_Duel::agents[-1] and s.players[-1] — both out-of-bounds. Quieter version of the same bug: card_selection(state, -1, ...) is special-cased to mean "both players", so several effects silently target the opponent's cards too.

4. People cards can't be played by the rules at all. play_card handles WONDER and EVENT and has no branch for PEOPLE — the card leaves the hand and vanishes. The only code that ever appends to Game_State::peoples is agent_ui.cpp:26, in the app's UI layer, and only when the player exits Playground mode. Since scoring iterates peoples, headless play, self-play and the AI all score people at zero. The rules for the game's central card type live in the UI.

5. Setup never fills the player decks. quick_setup puts every card in shared_deck and deals hands from there; players[i].deck stays empty. models.cpp:163 ends the game as soon as a player passes with an empty deck, so games end after one or two decisions. My test runs bear this out — 1 to 9 choices per game.

6. shuffle_card_into_deck reseeds from random_device mid-game (gameplay.cpp:322). Game logic that draws fresh entropy makes the minimax search nondeterministic across copies of the same position, and desynchronizes online play permanently — the network protocol replays only action indices, so the two peers' decks diverge the moment that card fires.

7. minimax_scores contradicts its own comment. The comment says each root action gets a full [-inf, +inf] window "so the scores are exact values … which matters when several actions tie for best." The code below it seeds each search with shared_alpha (minimax.h:157), so cut-off actions come back as bounds, not values. A cut action can return exactly alpha and tie with the true best, and argmax_randomized will then sometimes pick the worse move. The unsynchronized float write across threads is separately UB, though the existing ponytail: comment shows that one was a deliberate call.

Architectural changes
Pick one protocol for advancing a game. This is the highest-leverage change and it's what bug 2 is made of. Right now Choice::resolve returns the next choice and games also expose next_choice(), and the two are only consistent when next_choice() is a pure function of the state. I'd delete next_choice() from the interface and make resolve the single source of truth, since the game loop already works that way and it's the one that games can't avoid implementing. Then Game::_choice is genuinely the pending choice, resolve_choice means something, and search and play cannot disagree by construction. The alternative — delete the return value from resolve and make next_choice() mandatory and pure — also works, but it forces Gods' queue-draining into next_choice() where it's harder to see.

While you're there: next_choice(), evaluate_state() and sample_state() form an unwritten contract that every game must satisfy, discovered only through template errors. Three declarations in one header next to struct Game would make the thing a new game has to implement legible in ten seconds.

Get std::function out of the state that search copies. Choice holds two std::functions, each capturing more std::functions (gameplay.cpp:79-86), and Game_State::queue is a std::vector<Choice>. Minimax copies the whole Game_State once per child per node, so every node copies a pile of type-erased heap allocations. The comments in game.h argue carefully that inline target arrays avoid allocation on the hot path — but pack_targets returns a std::vector<int>, and get_targets returns std::vector<Card_Id> where Card_Id holds a std::string. The stated design goal is defeated three layers down. Making Card_Id::area the area_code enum that already exists in models.h:60 is a contained change that removes a string compare and a heap allocation from every action enumeration.

Move the Gods setup into gods/. quick_setup and load_card_designs live in gods_app/main.cpp, so the rules library can't be exercised without the graphical app. That's why bugs 1, 4 and 5 could sit there — there's no way to write a test that starts a game. I had to reimplement quick_setup in my scratch test just to reach the rules. Moving those two functions into gods/setup.cpp makes the library independently runnable and testable, which is the precondition for everything else.

Put Gods in a namespace. Chess, Dot, Scopa, Tressette and Connect Four each wrap their Game_State in a namespace; Gods puts Game_State, Card, Player, Card_Id, play_card and compute_player_score in the global namespace. That's why main.cpp:14-19 needs a comment about include ordering versus raylib's color macros, and it means Gods can't share a translation unit with another game.

Replace stacks_offset with names. The UI reaches into the scene tree by position: base + player_index * 5, with a struct documenting which of the five slots is which (agent_ui.h:22). Inserting one thing earlier in the list silently corrupts every zone. main.cpp:122 already builds a name-to-id map for the same scene tree, so both mechanisms exist side by side. Keep the map, delete the arithmetic.

Layout should read Table_State::width/height, not tt::WINDOW_WIDTH. This is the task.md item. Table_State already carries the real window size; the HUD and layout code use the compile-time constants instead (ui.cpp:163, ui.cpp:174, play.cpp:96). Threading the actual dimensions through is a mechanical change, and place_next/place_inside already do the right thing once they're given honest numbers.

Deletions, cheapest wins first: game/mcts_old.h (349 lines, not included anywhere), Choose_Options (never constructed; agent_ui.cpp:103 has a branch for it that can't fire), Choose_Card::up_to (action_options_count never reads it), resolve_pending_trick in Tressette (its own comment says it's unused), and has_card_class.

Two smaller notes: card_designs is a std::vector<std::unique_ptr<Card_Design>> (models.h:211), which your CLAUDE.md rules out — a std::vector<Card_Design*> or a variant would fit the stated style. And Timing_Agent in agent.h uses <chrono>, which is only included outside #ifndef __EMSCRIPTEN__, so it won't compile for web.

My verification programs are in the scratchpad if you want to rerun them; nothing in the repository was modified.