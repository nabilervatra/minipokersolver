#pragma once

#include "poker/tree.hpp"

#include <string>
#include <vector>

namespace poker::detail {

struct Transition {
    TreeState state;
    bool via_chance = false;
    bool is_terminal = false;
    TerminalKind terminal_kind = TerminalKind::Showdown;
};

int street_index(Street s);
std::string state_key(const TreeState& s);
TreeState initial_state(const BettingAbstraction& ab);
std::vector<Action> legal_actions(const TreeState& s, const BettingAbstraction& ab);
Transition apply_action(const TreeState& input, const Action& a);
TerminalData terminal_from_state(const TreeState& s, TerminalKind kind);

} // namespace poker::detail
