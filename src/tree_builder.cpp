#include "poker/tree.hpp"

#include "tree_state_logic.hpp"

#include <stdexcept>
#include <unordered_map>

namespace poker {

namespace {

struct BuildContext {
    const BettingAbstraction& ab;
    std::size_t max_nodes;

    GameTree tree;
    std::unordered_map<std::string, int> memo;

    int make_node(const TreeNode& n) {
        if (tree.nodes.size() >= max_nodes) {
            throw std::runtime_error("tree build exceeded max_nodes; refine abstraction or increase limit");
        }
        tree.nodes.push_back(n);
        return n.id;
    }

    int build_terminal(const TreeState& s, TerminalKind kind) {
        const std::string key = std::string("T:") + (kind == TerminalKind::Fold ? "F:" : "S:") + detail::state_key(s);
        auto it = memo.find(key);
        if (it != memo.end()) {
            return it->second;
        }

        TreeNode n;
        n.id = static_cast<int>(tree.nodes.size());
        n.type = NodeType::Terminal;
        n.key = key;
        n.state = s;
        n.terminal = detail::terminal_from_state(s, kind);

        const int id = make_node(n);
        memo.emplace(key, id);
        return id;
    }

    int build_chance(const TreeState& s) {
        const std::string key = "C:" + detail::state_key(s);
        auto it = memo.find(key);
        if (it != memo.end()) {
            return it->second;
        }

        TreeNode n;
        n.id = static_cast<int>(tree.nodes.size());
        n.type = NodeType::Chance;
        n.key = key;
        n.state = s;

        const int id = make_node(n);
        memo.emplace(key, id);

        const int child = build_decision_or_terminal(s);
        tree.nodes[static_cast<std::size_t>(id)].children.push_back(child);
        return id;
    }

    int build_decision_or_terminal(const TreeState& s) {
        if (s.street == Street::Terminal) {
            const TerminalKind kind = (s.folded[0] || s.folded[1]) ? TerminalKind::Fold : TerminalKind::Showdown;
            return build_terminal(s, kind);
        }

        const std::string key = "D:" + detail::state_key(s);
        auto it = memo.find(key);
        if (it != memo.end()) {
            return it->second;
        }

        TreeNode n;
        n.id = static_cast<int>(tree.nodes.size());
        n.type = NodeType::Decision;
        n.key = key;
        n.state = s;

        const int id = make_node(n);
        memo.emplace(key, id);

        auto actions = detail::legal_actions(s, ab);
        for (const auto& a : actions) {
            detail::Transition t = detail::apply_action(s, a);
            int child = -1;
            if (t.is_terminal) {
                child = build_terminal(t.state, t.terminal_kind);
            } else if (t.via_chance) {
                child = build_chance(t.state);
            } else {
                child = build_decision_or_terminal(t.state);
            }
            tree.nodes[static_cast<std::size_t>(id)].actions.push_back(a);
            tree.nodes[static_cast<std::size_t>(id)].children.push_back(child);
        }

        return id;
    }
};

} // namespace

TreeBuilder::TreeBuilder(BettingAbstraction abstraction) : abstraction_(std::move(abstraction)) {}

GameTree TreeBuilder::build(std::size_t max_nodes) const {
    BuildContext ctx{abstraction_, max_nodes, GameTree{}, {}};
    TreeState root = detail::initial_state(abstraction_);
    ctx.tree.root_id = ctx.build_decision_or_terminal(root);
    return std::move(ctx.tree);
}

BettingAbstraction TreeBuilder::default_abstraction() {
    return BettingAbstraction{};
}

std::string to_string(NodeType t) {
    switch (t) {
        case NodeType::Decision:
            return "Decision";
        case NodeType::Chance:
            return "Chance";
        case NodeType::Terminal:
            return "Terminal";
    }
    return "Unknown";
}

std::string to_string(TerminalKind t) {
    switch (t) {
        case TerminalKind::Fold:
            return "Fold";
        case TerminalKind::Showdown:
            return "Showdown";
    }
    return "Unknown";
}

} // namespace poker
