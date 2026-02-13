#include "poker/engine.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <numeric>
#include <sstream>

namespace poker {

namespace {

int rank_of(int card) {
    return (card % 13) + 2; // 2..14
}

int suit_of(int card) {
    return card / 13; // 0..3
}

int pack_score(int category, const std::vector<int>& kickers_desc) {
    // Category in [0..8], larger is better.
    // Pack as fixed-width base-15 number: category then exactly 5 kicker slots.
    // Fixed width is required so category always dominates cross-category compares.
    int score = category;
    for (int i = 0; i < 5; ++i) {
        const int k = i < static_cast<int>(kickers_desc.size()) ? kickers_desc[static_cast<std::size_t>(i)] : 0;
        score = score * 15 + k;
    }
    return score;
}

int evaluate_5cards(const std::array<int, 5>& cards) {
    std::array<int, 15> rank_count{};
    std::array<int, 4> suit_count{};
    std::vector<int> ranks;
    ranks.reserve(5);

    for (int c : cards) {
        const int r = rank_of(c);
        const int s = suit_of(c);
        rank_count[r]++;
        suit_count[s]++;
        ranks.push_back(r);
    }

    std::sort(ranks.begin(), ranks.end(), std::greater<int>());

    const bool is_flush = std::any_of(suit_count.begin(), suit_count.end(), [](int c) { return c == 5; });

    std::vector<int> unique_ranks = ranks;
    unique_ranks.erase(std::unique(unique_ranks.begin(), unique_ranks.end()), unique_ranks.end());
    std::sort(unique_ranks.begin(), unique_ranks.end());

    bool is_straight = false;
    int straight_high = 0;
    if (unique_ranks.size() == 5) {
        if (unique_ranks.back() - unique_ranks.front() == 4) {
            is_straight = true;
            straight_high = unique_ranks.back();
        } else if (unique_ranks == std::vector<int>{2, 3, 4, 5, 14}) {
            is_straight = true;
            straight_high = 5;
        }
    }

    if (is_straight && is_flush) {
        return pack_score(8, {straight_high});
    }

    std::vector<int> fours;
    std::vector<int> threes;
    std::vector<int> pairs;
    std::vector<int> singles;

    for (int r = 14; r >= 2; --r) {
        if (rank_count[r] == 4) {
            fours.push_back(r);
        } else if (rank_count[r] == 3) {
            threes.push_back(r);
        } else if (rank_count[r] == 2) {
            pairs.push_back(r);
        } else if (rank_count[r] == 1) {
            singles.push_back(r);
        }
    }

    if (!fours.empty()) {
        return pack_score(7, {fours[0], singles[0]});
    }

    if (!threes.empty() && !pairs.empty()) {
        return pack_score(6, {threes[0], pairs[0]});
    }

    if (is_flush) {
        return pack_score(5, ranks);
    }

    if (is_straight) {
        return pack_score(4, {straight_high});
    }

    if (!threes.empty()) {
        return pack_score(3, {threes[0], singles[0], singles[1]});
    }

    if (pairs.size() >= 2) {
        return pack_score(2, {pairs[0], pairs[1], singles[0]});
    }

    if (pairs.size() == 1) {
        return pack_score(1, {pairs[0], singles[0], singles[1], singles[2]});
    }

    return pack_score(0, ranks);
}

} // namespace

std::string to_string(Street street) {
    switch (street) {
        case Street::Preflop:
            return "Preflop";
        case Street::Flop:
            return "Flop";
        case Street::Turn:
            return "Turn";
        case Street::River:
            return "River";
        case Street::Showdown:
            return "Showdown";
        case Street::Terminal:
            return "Terminal";
    }
    return "Unknown";
}

std::string to_string(ActionType type) {
    switch (type) {
        case ActionType::Fold:
            return "Fold";
        case ActionType::Check:
            return "Check";
        case ActionType::Call:
            return "Call";
        case ActionType::Bet:
            return "Bet";
        case ActionType::Raise:
            return "Raise";
    }
    return "Unknown";
}

Engine::Engine(unsigned int seed) : rng_(seed) {}

int Engine::draw_card(State& state) {
    std::uniform_int_distribution<int> dist(0, 51);
    while (true) {
        const int c = dist(rng_);
        if (!state.used_cards[c]) {
            state.used_cards[c] = true;
            return c;
        }
    }
}

State Engine::new_hand(int starting_stack, int small_blind, int big_blind) {
    State s;
    s.street = Street::Preflop;
    s.stacks = {starting_stack, starting_stack};
    s.to_act = 0;

    // Heads-up: SB is player 0, BB is player 1.
    s.stacks[0] -= small_blind;
    s.stacks[1] -= big_blind;
    s.committed_this_round[0] = small_blind;
    s.committed_this_round[1] = big_blind;
    s.committed_total[0] = small_blind;
    s.committed_total[1] = big_blind;
    s.current_bet = big_blind;
    s.bet_to_call = big_blind - small_blind;
    s.last_bet_size = big_blind - small_blind;
    s.pot = small_blind + big_blind;

    for (int p = 0; p < 2; ++p) {
        s.hole_cards[p][0] = draw_card(s);
        s.hole_cards[p][1] = draw_card(s);
    }

    return s;
}

int Engine::min_raise_to(const State& state) const {
    const int min_raise_size = std::max(1, state.last_bet_size);
    return state.current_bet + min_raise_size;
}

std::vector<Action> Engine::legal_actions(const State& state) const {
    std::vector<Action> out;
    if (state.street == Street::Terminal || state.street == Street::Showdown) {
        return out;
    }

    const int player = state.to_act;
    const int stack = state.stacks[player];
    const int call_amount = std::max(0, state.current_bet - state.committed_this_round[player]);

    if (call_amount > 0) {
        out.push_back(Action{player, ActionType::Fold, 0, call_amount, state.street});
        out.push_back(Action{player, ActionType::Call, std::min(call_amount, stack), call_amount, state.street});

        if (stack > call_amount) {
            const int min_to = min_raise_to(state);
            const std::array<double, 3> raise_sizes = {0.5, 1.0, 2.0};
            for (double x : raise_sizes) {
                const int target = std::max(min_to, state.current_bet + static_cast<int>(state.pot * x));
                const int needed = target - state.committed_this_round[player];
                if (needed > call_amount && needed < stack) {
                    out.push_back(Action{player, ActionType::Raise, needed, call_amount, state.street});
                }
            }
            out.push_back(Action{player, ActionType::Raise, stack, call_amount, state.street}); // all-in
        }
    } else {
        out.push_back(Action{player, ActionType::Check, 0, 0, state.street});
        if (stack > 0) {
            const std::array<double, 3> bet_sizes = {0.5, 1.0, 2.0};
            for (double x : bet_sizes) {
                int amount = std::max(1, static_cast<int>(state.pot * x));
                if (amount < stack) {
                    out.push_back(Action{player, ActionType::Bet, amount, 0, state.street});
                }
            }
            out.push_back(Action{player, ActionType::Bet, stack, 0, state.street}); // all-in
        }
    }

    // Remove exact duplicates.
    std::sort(out.begin(), out.end(), [](const Action& a, const Action& b) {
        if (a.type != b.type) return a.type < b.type;
        return a.amount < b.amount;
    });
    out.erase(std::unique(out.begin(), out.end(), [](const Action& a, const Action& b) {
        return a.type == b.type && a.amount == b.amount;
    }), out.end());

    return out;
}

bool Engine::is_round_closed(const State& state) const {
    if (state.folded[0] || state.folded[1]) {
        return true;
    }
    return state.committed_this_round[0] == state.committed_this_round[1];
}

void Engine::advance_street(State& state) {
    state.bet_to_call = 0;
    state.current_bet = 0;
    state.last_bet_size = 0;
    state.committed_this_round = {0, 0};

    if (state.street == Street::Preflop) {
        state.street = Street::Flop;
        state.board.push_back(draw_card(state));
        state.board.push_back(draw_card(state));
        state.board.push_back(draw_card(state));
    } else if (state.street == Street::Flop) {
        state.street = Street::Turn;
        state.board.push_back(draw_card(state));
    } else if (state.street == Street::Turn) {
        state.street = Street::River;
        state.board.push_back(draw_card(state));
    } else if (state.street == Street::River) {
        state.street = Street::Showdown;
    }

    // Postflop first action is out-of-position player in heads-up (player 0).
    state.to_act = 0;
}

void Engine::deal_remaining_board(State& state) {
    while (state.board.size() < 5) {
        state.board.push_back(draw_card(state));
    }
}

bool Engine::apply_action(State& state, const Action& action) {
    const std::vector<Action> legals = legal_actions(state);
    const bool legal = std::any_of(legals.begin(), legals.end(), [&](const Action& a) {
        return a.type == action.type && a.amount == action.amount && a.player == action.player;
    });
    if (!legal) {
        return false;
    }

    state.history.push_back(action);
    const int p = action.player;
    const int opp = 1 - p;
    const auto force_allin_showdown = [&]() {
        if (!state.folded[0] && !state.folded[1] && (state.stacks[0] == 0 || state.stacks[1] == 0)) {
            deal_remaining_board(state);
            state.street = Street::Terminal;
            state.to_act = 0;
            state.bet_to_call = 0;
            state.current_bet = 0;
            state.last_bet_size = 0;
            state.committed_this_round = {0, 0};
            return true;
        }
        return false;
    };

    if (action.type == ActionType::Fold) {
        state.folded[p] = true;
        state.street = Street::Terminal;
        return true;
    }

    if (action.type == ActionType::Check) {
        if (force_allin_showdown()) {
            return true;
        }
        if (is_round_closed(state) && state.history.size() >= 2 && state.history[state.history.size() - 2].street == state.street) {
            advance_street(state);
            if (state.street == Street::Showdown) {
                deal_remaining_board(state);
                state.street = Street::Terminal;
            }
        } else {
            state.to_act = opp;
        }
        return true;
    }

    if (action.type == ActionType::Call) {
        const int put = std::min(action.amount, state.stacks[p]);
        state.stacks[p] -= put;
        state.committed_this_round[p] += put;
        state.committed_total[p] += put;
        state.pot += put;
        state.bet_to_call = std::max(0, state.current_bet - state.committed_this_round[opp]);

        if (force_allin_showdown()) {
            return true;
        }

        if (is_round_closed(state)) {
            advance_street(state);
            if (state.street == Street::Showdown) {
                deal_remaining_board(state);
                state.street = Street::Terminal;
            }
        } else {
            state.to_act = opp;
        }
        return true;
    }

    if (action.type == ActionType::Bet || action.type == ActionType::Raise) {
        const int put = std::min(action.amount, state.stacks[p]);
        const int prev_commit = state.committed_this_round[p];
        state.stacks[p] -= put;
        state.committed_this_round[p] += put;
        state.committed_total[p] += put;
        state.pot += put;

        const int new_bet = state.committed_this_round[p];
        const int prior_current = state.current_bet;
        state.current_bet = std::max(state.current_bet, new_bet);
        state.last_bet_size = std::max(1, state.current_bet - prior_current);
        state.bet_to_call = std::max(0, state.current_bet - state.committed_this_round[opp]);

        if (force_allin_showdown()) {
            return true;
        }

        (void)prev_commit;
        state.to_act = opp;
        return true;
    }

    return false;
}

int Engine::evaluate_7card(const std::array<int, 2>& hole, const std::vector<int>& board) const {
    assert(board.size() == 5);
    std::array<int, 7> all{};
    all[0] = hole[0];
    all[1] = hole[1];
    for (int i = 0; i < 5; ++i) {
        all[i + 2] = board[i];
    }

    int best = -1;
    for (int a = 0; a < 7; ++a) {
        for (int b = a + 1; b < 7; ++b) {
            std::array<int, 5> five{};
            int idx = 0;
            for (int i = 0; i < 7; ++i) {
                if (i == a || i == b) continue;
                five[idx++] = all[i];
            }
            best = std::max(best, evaluate_5cards(five));
        }
    }
    return best;
}

TerminalResult Engine::terminal_payoff(const State& state) const {
    TerminalResult r;
    if (state.street != Street::Terminal) {
        return r;
    }

    r.is_terminal = true;

    std::array<int, 2> payout{0, 0};

    if (state.folded[0] != state.folded[1]) {
        const int winner = state.folded[0] ? 1 : 0;
        r.winner = winner;
        r.reason = "fold";
        payout[winner] = state.pot;
    } else {
        const int s0 = evaluate_7card(state.hole_cards[0], state.board);
        const int s1 = evaluate_7card(state.hole_cards[1], state.board);
        r.reason = "showdown";

        if (s0 > s1) {
            r.winner = 0;
            payout[0] = state.pot;
        } else if (s1 > s0) {
            r.winner = 1;
            payout[1] = state.pot;
        } else {
            r.winner = -1;
            payout[0] = state.pot / 2;
            payout[1] = state.pot - payout[0];
        }
    }

    r.chip_delta[0] = payout[0] - state.committed_total[0];
    r.chip_delta[1] = payout[1] - state.committed_total[1];

    return r;
}

Action Engine::random_legal_action(const State& state) {
    auto legals = legal_actions(state);
    std::uniform_int_distribution<std::size_t> dist(0, legals.size() - 1);
    return legals[dist(rng_)];
}

} // namespace poker
