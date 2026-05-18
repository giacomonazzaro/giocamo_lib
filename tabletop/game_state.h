#pragma once
#include "models.h"

// Thing classification helpers.
bool is_card(const Thing& t);
bool is_container(const Thing& t);

// Tree navigation helpers (scene tree is rooted at state.things[state.root]).
// find_parent returns the thing-id of the parent, or -1 if not found.
int       find_parent(int thing_id, const Table_State& state);
Vector2   local_to_world(int thing_id, const Table_State& state);
Rectangle world_rect(int thing_id, const Table_State& state);

// Stack/card operations. stack_id and card_id are both thing-ids.
Thing create_card_design(int id);
void  add_card_to_stack(int card_id, int stack_id, Table_State& state);
int   remove_card_from_stack(int card_id, int stack_id, Table_State& state);
void  move_card_to_stack(
   int card_id, int from_stack_id, int to_stack_id, Table_State& state
 );
void update_card_positions(int stack_id, Table_State& state, bool sort);
int  find_stack_containing_card(int card_id, const Table_State& state);
// Loose cards live as direct children of the root thing.
void             add_loose_card(int card_id, Table_State& state);
int              remove_loose_card(int card_id, Table_State& state);
std::vector<int> create_sample_cards(Table_State& state);
