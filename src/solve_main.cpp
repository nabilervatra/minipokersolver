#include "poker/tree.hpp"

#include <array>
#include <iostream>

int main() {
    poker::BettingAbstraction ab = poker::TreeBuilder::default_abstraction();

    // Keep first solver tree manageable while still non-trivial.
    ab.max_raises_per_street = 2;
    ab.bet_sizes_by_street = {
        std::vector<double>{0.5, 1.0},
        std::vector<double>{0.5, 1.0},
        std::vector<double>{1.0},
        std::vector<double>{1.0}
    };
    ab.raise_sizes_by_street = ab.bet_sizes_by_street;

    poker::TreeBuilder builder(ab);
    poker::GameTree tree = builder.build(300000);

    std::array<int, 3> type_counts{0, 0, 0};
    int fold_terminal = 0;
    int showdown_terminal = 0;

    for (const auto& n : tree.nodes) {
        if (n.type == poker::NodeType::Decision) {
            type_counts[0]++;
        } else if (n.type == poker::NodeType::Chance) {
            type_counts[1]++;
        } else if (n.type == poker::NodeType::Terminal) {
            type_counts[2]++;
            if (n.terminal.kind == poker::TerminalKind::Fold) {
                fold_terminal++;
            } else {
                showdown_terminal++;
            }
        }
    }

    std::cout << "Tree build complete\n";
    std::cout << "root_id: " << tree.root_id << "\n";
    std::cout << "total_nodes: " << tree.nodes.size() << "\n";
    std::cout << "decision_nodes: " << type_counts[0] << "\n";
    std::cout << "chance_nodes: " << type_counts[1] << "\n";
    std::cout << "terminal_nodes: " << type_counts[2] << "\n";
    std::cout << "terminal_fold: " << fold_terminal << "\n";
    std::cout << "terminal_showdown: " << showdown_terminal << "\n";

    return 0;
}
