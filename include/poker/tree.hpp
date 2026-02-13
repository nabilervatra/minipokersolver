#pragma once

#include "poker/types.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace poker {

enum class NodeType {
    Decision,
    Chance,
    Terminal
};

enum class TerminalKind {
    Fold,
    Showdown
};

struct BettingAbstraction {
    int starting_stack = 1000;
    int small_blind = 5;
    int big_blind = 10;

    // Max number of aggressive actions (bet/raise) allowed per street.
    int max_raises_per_street = 2;
    bool allow_all_in = true;

    // Indexed by street: 0=Preflop, 1=Flop, 2=Turn, 3=River.
    std::array<std::vector<double>, 4> bet_sizes_by_street{
        std::vector<double>{0.5, 1.0, 2.0},
        std::vector<double>{0.5, 1.0, 2.0},
        std::vector<double>{0.5, 1.0, 2.0},
        std::vector<double>{0.5, 1.0, 2.0}
    };

    std::array<std::vector<double>, 4> raise_sizes_by_street{
        std::vector<double>{0.5, 1.0, 2.0},
        std::vector<double>{0.5, 1.0, 2.0},
        std::vector<double>{0.5, 1.0, 2.0},
        std::vector<double>{0.5, 1.0, 2.0}
    };
};

struct TreeState {
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
    std::array<bool, 2> acted_this_round{false, false};
    int raises_this_street = 0;
};

struct TerminalData {
    TerminalKind kind = TerminalKind::Showdown;
    int winner = -1; // Fold winner known; showdown winner unresolved at tree-build stage.
    int pot = 0;
    std::array<int, 2> committed_total{0, 0};
    // Filled for fold terminals only. Showdown utility is resolved later by range/equity code.
    std::array<int, 2> chip_delta_if_forced{0, 0};
};

struct TreeNode {
    int id = -1;
    NodeType type = NodeType::Decision;
    std::string key;
    TreeState state;

    // For Decision nodes: actions[i] leads to children[i].
    std::vector<Action> actions;
    std::vector<int> children;

    // For Terminal nodes.
    TerminalData terminal;
};

struct GameTree {
    int root_id = -1;
    std::vector<TreeNode> nodes;
};

class TreeBuilder {
public:
    explicit TreeBuilder(BettingAbstraction abstraction);

    GameTree build(std::size_t max_nodes = 200000) const;

    static BettingAbstraction default_abstraction();

private:
    BettingAbstraction abstraction_;
};

std::string to_string(NodeType t);
std::string to_string(TerminalKind t);

} // namespace poker
