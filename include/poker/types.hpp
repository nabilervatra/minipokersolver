#pragma once

#include <array>
#include <string>
#include <vector>

namespace poker {

enum class Street {
    Preflop,
    Flop,
    Turn,
    River,
    Showdown,
    Terminal
};

enum class ActionType {
    Fold,
    Check,
    Call,
    Bet,
    Raise
};

struct Action {
    int player = 0;
    ActionType type = ActionType::Check;
    int amount = 0;
    int to_call_before = 0;
    Street street = Street::Preflop;
};

struct State {
    Street street = Street::Preflop;
    int pot = 0;
    std::array<int, 2> stacks{0, 0};
    int to_act = 0;
    int bet_to_call = 0;
    int last_bet_size = 0;
    int current_bet = 0;
    std::array<int, 2> committed_this_round{0, 0};
    std::array<int, 2> committed_total{0, 0};
    std::array<bool, 2> folded{false, false};
    std::vector<Action> history;

    std::array<std::array<int, 2>, 2> hole_cards{};
    std::vector<int> board;
    std::array<bool, 52> used_cards{};
};

struct TerminalResult {
    bool is_terminal = false;
    int winner = -1;
    std::array<int, 2> chip_delta{0, 0};
    std::string reason;
};

std::string to_string(Street street);
std::string to_string(ActionType type);

} // namespace poker
