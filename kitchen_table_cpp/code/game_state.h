#pragma once
#include "models.h"

KT_Card create_card_design(int id);
void add_card_to_stack(int card_id, Stack& stack, Table_State& state);
int  remove_card_from_stack(int card_id, Stack& stack, Table_State& state);
void move_card_to_stack(int card_id, Stack& from, Stack& to, Table_State& state);
void update_card_positions(Stack& stack, Table_State& state, bool sort);
int  find_stack_containing_card(int card_id, const Table_State& state);
void add_loose_card(int card_id, Table_State& state);
int  remove_loose_card(int card_id, Table_State& state);
std::vector<int> create_sample_cards(Table_State& state);
