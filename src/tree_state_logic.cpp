#include "tree_state_logic.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>

namespace poker::detail {

int street_index(Street s) {
    switch (s) {
        case Street::Preflop:
            return 0;
        case Street::Flop:
            return 1;
        case Street::Turn:
            return 2;
        case Street::River:
            return 3;
        case Street::Showdown:
            return 4;
        case Street::Terminal:
            return 5;
    }
    return -1;
}

std::string state_key(const TreeState& s) {
    std::ostringstream os;
    os << street_index(s.street)
       << "|" << s.pot
       << "|" << s.stacks[0] << "," << s.stacks[1]
       << "|" << s.to_act
       << "|" << s.bet_to_call
       << "|" << s.last_bet_size
       << "|" << s.current_bet
       << "|" << s.committed_this_round[0] << "," << s.committed_this_round[1]
       << "|" << s.committed_total[0] << "," << s.committed_total[1]
       << "|" << static_cast<int>(s.folded[0]) << "," << static_cast<int>(s.folded[1])
       << "|" << static_cast<int>(s.acted_this_round[0]) << "," << static_cast<int>(s.acted_this_round[1])
       << "|" << s.raises_this_street;
    return os.str();
}

TreeState initial_state(const BettingAbstraction& ab) {
    TreeState s;
    s.street = Street::Preflop;
    s.stacks = {ab.starting_stack, ab.starting_stack};

    s.stacks[0] -= ab.small_blind;
    s.stacks[1] -= ab.big_blind;
    s.committed_this_round = {ab.small_blind, ab.big_blind};
    s.committed_total = {ab.small_blind, ab.big_blind};

    s.pot = ab.small_blind + ab.big_blind;
    s.current_bet = ab.big_blind;
    s.bet_to_call = ab.big_blind - ab.small_blind;
    s.last_bet_size = ab.big_blind - ab.small_blind;
    s.to_act = 0;
    s.acted_this_round = {false, false};
    s.raises_this_street = 0;

    return s;
}

namespace {

void advance_street(TreeState& s) {
    s.bet_to_call = 0;
    s.current_bet = 0;
    s.last_bet_size = 0;
    s.committed_this_round = {0, 0};
    s.acted_this_round = {false, false};
    s.raises_this_street = 0;

    if (s.street == Street::Preflop) {
        s.street = Street::Flop;
    } else if (s.street == Street::Flop) {
        s.street = Street::Turn;
    } else if (s.street == Street::Turn) {
        s.street = Street::River;
    } else if (s.street == Street::River) {
        s.street = Street::Terminal;
    }

    s.to_act = 0;
}

int min_raise_to(const TreeState& s) {
    const int min_raise_size = std::max(1, s.last_bet_size);
    return s.current_bet + min_raise_size;
}

bool is_round_closed(const TreeState& s) {
    if (s.folded[0] || s.folded[1]) {
        return true;
    }
    return s.committed_this_round[0] == s.committed_this_round[1] && s.acted_this_round[0] && s.acted_this_round[1];
}

void finish_showdown(Transition& t) {
    t.state.street = Street::Terminal;
    t.state.to_act = 0;
    t.state.bet_to_call = 0;
    t.state.current_bet = 0;
    t.state.last_bet_size = 0;
    t.state.committed_this_round = {0, 0};
    t.state.acted_this_round = {false, false};
    t.is_terminal = true;
    t.terminal_kind = TerminalKind::Showdown;
}

} // namespace

std::vector<Action> legal_actions(const TreeState& s, const BettingAbstraction& ab) {
    std::vector<Action> out;
    if (s.street == Street::Terminal || s.street == Street::Showdown) {
        return out;
    }

    const int si = street_index(s.street);
    if (si < 0 || si > 3) {
        return out;
    }

    const int p = s.to_act;
    const int stack = s.stacks[p];
    const int call_amount = std::max(0, s.current_bet - s.committed_this_round[p]);

    if (call_amount > 0) {
        out.push_back(Action{p, ActionType::Fold, 0, call_amount, s.street});
        out.push_back(Action{p, ActionType::Call, std::min(call_amount, stack), call_amount, s.street});

        if (stack > call_amount && s.raises_this_street < ab.max_raises_per_street) {
            const int min_to = min_raise_to(s);
            for (double x : ab.raise_sizes_by_street[static_cast<std::size_t>(si)]) {
                const int target = std::max(min_to, s.current_bet + static_cast<int>(s.pot * x));
                const int needed = target - s.committed_this_round[p];
                if (needed > call_amount && needed < stack) {
                    out.push_back(Action{p, ActionType::Raise, needed, call_amount, s.street});
                }
            }
            if (ab.allow_all_in) {
                out.push_back(Action{p, ActionType::Raise, stack, call_amount, s.street});
            }
        }
    } else {
        out.push_back(Action{p, ActionType::Check, 0, 0, s.street});

        if (stack > 0 && s.raises_this_street < ab.max_raises_per_street) {
            for (double x : ab.bet_sizes_by_street[static_cast<std::size_t>(si)]) {
                const int amount = std::max(1, static_cast<int>(s.pot * x));
                if (amount < stack) {
                    out.push_back(Action{p, ActionType::Bet, amount, 0, s.street});
                }
            }
            if (ab.allow_all_in) {
                out.push_back(Action{p, ActionType::Bet, stack, 0, s.street});
            }
        }
    }

    std::sort(out.begin(), out.end(), [](const Action& a, const Action& b) {
        if (a.type != b.type) return a.type < b.type;
        return a.amount < b.amount;
    });
    out.erase(std::unique(out.begin(), out.end(), [](const Action& a, const Action& b) {
        return a.type == b.type && a.amount == b.amount;
    }), out.end());

    return out;
}

Transition apply_action(const TreeState& input, const Action& a) {
    Transition t;
    t.state = input;

    const int p = a.player;
    const int opp = 1 - p;

    if (a.type == ActionType::Fold) {
        t.state.folded[p] = true;
        t.state.street = Street::Terminal;
        t.state.to_act = opp;
        t.state.bet_to_call = 0;
        t.state.current_bet = 0;
        t.state.last_bet_size = 0;
        t.state.committed_this_round = {0, 0};
        t.state.acted_this_round = {false, false};
        t.is_terminal = true;
        t.terminal_kind = TerminalKind::Fold;
        return t;
    }

    if (a.type == ActionType::Check) {
        t.state.acted_this_round[p] = true;
        if (is_round_closed(t.state)) {
            advance_street(t.state);
            if (t.state.street == Street::Terminal) {
                t.is_terminal = true;
                t.terminal_kind = TerminalKind::Showdown;
            } else {
                t.via_chance = true;
            }
        } else {
            t.state.to_act = opp;
            t.state.bet_to_call = std::max(0, t.state.current_bet - t.state.committed_this_round[opp]);
        }
        return t;
    }

    if (a.type == ActionType::Call) {
        const int put = std::min(a.amount, t.state.stacks[p]);
        t.state.stacks[p] -= put;
        t.state.committed_this_round[p] += put;
        t.state.committed_total[p] += put;
        t.state.pot += put;
        t.state.acted_this_round[p] = true;

        if (!t.state.folded[0] && !t.state.folded[1] && (t.state.stacks[0] == 0 || t.state.stacks[1] == 0)) {
            finish_showdown(t);
            return t;
        }

        if (is_round_closed(t.state)) {
            advance_street(t.state);
            if (t.state.street == Street::Terminal) {
                t.is_terminal = true;
                t.terminal_kind = TerminalKind::Showdown;
            } else {
                t.via_chance = true;
            }
        } else {
            t.state.to_act = opp;
            t.state.bet_to_call = std::max(0, t.state.current_bet - t.state.committed_this_round[opp]);
        }
        return t;
    }

    if (a.type == ActionType::Bet || a.type == ActionType::Raise) {
        const int put = std::min(a.amount, t.state.stacks[p]);
        t.state.stacks[p] -= put;
        t.state.committed_this_round[p] += put;
        t.state.committed_total[p] += put;
        t.state.pot += put;

        const int new_bet = t.state.committed_this_round[p];
        const int prior_current = t.state.current_bet;
        t.state.current_bet = std::max(t.state.current_bet, new_bet);
        t.state.last_bet_size = std::max(1, t.state.current_bet - prior_current);
        t.state.bet_to_call = std::max(0, t.state.current_bet - t.state.committed_this_round[opp]);

        t.state.acted_this_round[p] = true;
        t.state.acted_this_round[opp] = false;
        t.state.raises_this_street += 1;
        t.state.to_act = opp;

        if (!t.state.folded[0] && !t.state.folded[1] && (t.state.stacks[0] == 0 || t.state.stacks[1] == 0)) {
            finish_showdown(t);
            return t;
        }

        return t;
    }

    throw std::runtime_error("unknown action type in apply_action");
}

TerminalData terminal_from_state(const TreeState& s, TerminalKind kind) {
    TerminalData t;
    t.kind = kind;
    t.pot = s.pot;
    t.committed_total = s.committed_total;

    if (kind == TerminalKind::Fold) {
        t.winner = s.folded[0] ? 1 : 0;
        std::array<int, 2> payout{0, 0};
        payout[t.winner] = s.pot;
        t.chip_delta_if_forced[0] = payout[0] - s.committed_total[0];
        t.chip_delta_if_forced[1] = payout[1] - s.committed_total[1];
    } else {
        t.winner = -1;
        t.chip_delta_if_forced = {0, 0};
    }

    return t;
}

} // namespace poker::detail
