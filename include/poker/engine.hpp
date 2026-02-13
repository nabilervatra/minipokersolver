#pragma once

#include "poker/types.hpp"

#include <random>
#include <vector>

namespace poker {

class Engine {
public:
    explicit Engine(unsigned int seed = 42);

    State new_hand(int starting_stack = 1000, int small_blind = 5, int big_blind = 10);

    std::vector<Action> legal_actions(const State& state) const;

    bool apply_action(State& state, const Action& action);

    TerminalResult terminal_payoff(const State& state) const;

    int evaluate_7card(const std::array<int, 2>& hole, const std::vector<int>& board) const;

    Action random_legal_action(const State& state);

private:
    std::mt19937 rng_;

    void advance_street(State& state);
    void deal_remaining_board(State& state);
    int draw_card(State& state);

    bool is_round_closed(const State& state) const;
    int min_raise_to(const State& state) const;
};

} // namespace poker
