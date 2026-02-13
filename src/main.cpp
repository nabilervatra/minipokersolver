#include "poker/engine.hpp"

#include <iostream>
#include <limits>
#include <vector>

namespace {

void print_terminal_state(int hand_index, const poker::State& state, const poker::TerminalResult& result) {
    const int start_p0 = state.stacks[0] + state.committed_total[0];
    const int start_p1 = state.stacks[1] + state.committed_total[1];
    const int settled_p0 = start_p0 + result.chip_delta[0];
    const int settled_p1 = start_p1 + result.chip_delta[1];

    std::cout << "=== Hand " << hand_index + 1 << " Terminal State ===\n";
    std::cout << "street: " << poker::to_string(state.street) << "\n";
    std::cout << "pot: " << state.pot << "\n";
    std::cout << "stacks_before_pot_award: [P0=" << state.stacks[0] << ", P1=" << state.stacks[1] << "]\n";
    std::cout << "committed_total: [P0=" << state.committed_total[0] << ", P1=" << state.committed_total[1] << "]\n";
    std::cout << "start_stacks_inferred: [P0=" << start_p0 << ", P1=" << start_p1 << "]\n";
    std::cout << "stacks_after_settlement: [P0=" << settled_p0 << ", P1=" << settled_p1 << "]\n";
    std::cout << "to_act: P" << state.to_act << "\n";
    std::cout << "bet_to_call: " << state.bet_to_call << "\n";
    std::cout << "last_bet_size: " << state.last_bet_size << "\n";

    std::cout << "board(cards as 0..51):";
    for (int c : state.board) std::cout << " " << c;
    std::cout << "\n";

    std::cout << "hole_p0: " << state.hole_cards[0][0] << " " << state.hole_cards[0][1] << "\n";
    std::cout << "hole_p1: " << state.hole_cards[1][0] << " " << state.hole_cards[1][1] << "\n";

    std::cout << "history:\n";
    for (const auto& a : state.history) {
        std::cout << "  [" << poker::to_string(a.street) << "] P" << a.player << " "
                  << poker::to_string(a.type) << " amount=" << a.amount
                  << " to_call_before=" << a.to_call_before << "\n";
    }

    std::cout << "result: reason=" << result.reason
              << ", winner=" << result.winner
              << ", chip_delta=[P0=" << result.chip_delta[0]
              << ", P1=" << result.chip_delta[1] << "]\n\n";
}

void print_legal_actions(const std::vector<poker::Action>& legals) {
    std::cout << "Legal actions:\n";
    for (std::size_t i = 0; i < legals.size(); ++i) {
        const auto& a = legals[i];
        std::cout << "  " << i << ": " << poker::to_string(a.type)
                  << " amount=" << a.amount
                  << " to_call_before=" << a.to_call_before << "\n";
    }
}

int read_int_with_prompt(const std::string& prompt, int min_v, int max_v) {
    while (true) {
        std::cout << prompt;
        int v = 0;
        if (std::cin >> v && v >= min_v && v <= max_v) {
            return v;
        }
        std::cout << "Invalid input. Enter a number in [" << min_v << ", " << max_v << "].\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

void run_interactive_hand(poker::Engine& engine, int human_player) {
    poker::State state = engine.new_hand();

    std::cout << "\nStarting interactive hand. You control P" << human_player << ".\n";
    std::cout << "Your hole cards (0..51): "
              << state.hole_cards[human_player][0] << " "
              << state.hole_cards[human_player][1] << "\n";

    int guard = 0;
    while (state.street != poker::Street::Terminal && guard < 200) {
        std::cout << "\nStreet: " << poker::to_string(state.street)
                  << " | Pot: " << state.pot
                  << " | To act: P" << state.to_act
                  << " | Bet to call: " << state.bet_to_call << "\n";

        auto legals = engine.legal_actions(state);
        poker::Action chosen{};

        if (state.to_act == human_player) {
            print_legal_actions(legals);
            const int idx = read_int_with_prompt("Choose action index: ", 0, static_cast<int>(legals.size()) - 1);
            chosen = legals[static_cast<std::size_t>(idx)];
        } else {
            chosen = engine.random_legal_action(state);
            std::cout << "Opponent chose: " << poker::to_string(chosen.type)
                      << " amount=" << chosen.amount << "\n";
        }

        if (!engine.apply_action(state, chosen)) {
            std::cerr << "Failed to apply action.\n";
            return;
        }
        ++guard;
    }

    if (guard >= 200) {
        std::cerr << "Guard reached; potential infinite loop.\n";
        return;
    }

    poker::TerminalResult result = engine.terminal_payoff(state);
    if (!result.is_terminal) {
        std::cerr << "Terminal payoff requested on non-terminal state.\n";
        return;
    }

    print_terminal_state(0, state, result);
}

} // namespace

int main() {
    poker::Engine engine(1337);

    const int mode = read_int_with_prompt("Select mode (0=interactive, 1=auto 10 hands): ", 0, 1);
    if (mode == 0) {
        const int human_player = read_int_with_prompt("Control which player? (0 or 1): ", 0, 1);
        run_interactive_hand(engine, human_player);
        return 0;
    }

    constexpr int kHands = 10;
    int folds = 0;
    int showdowns = 0;

    for (int h = 0; h < kHands; ++h) {
        poker::State state = engine.new_hand();

        int guard = 0;
        while (state.street != poker::Street::Terminal && guard < 200) {
            auto action = engine.random_legal_action(state);
            const bool ok = engine.apply_action(state, action);
            if (!ok) {
                std::cerr << "Illegal action selected; aborting hand " << h << "\n";
                return 1;
            }
            ++guard;
        }

        if (guard >= 200) {
            std::cerr << "Guard reached; potential infinite loop in hand " << h << "\n";
            return 2;
        }

        poker::TerminalResult result = engine.terminal_payoff(state);
        if (!result.is_terminal) {
            std::cerr << "Terminal payoff requested on non-terminal state\n";
            return 3;
        }
        
        if (result.reason == "fold") {
            ++folds;
        } else if (result.reason == "showdown") {
            ++showdowns;
        }

        print_terminal_state(h, state, result);
    }

    std::cout << "Simulated " << kHands << " hands successfully\n";
    std::cout << "fold outcomes: " << folds << "\n";
    std::cout << "showdown outcomes: " << showdowns << "\n";
    return 0;
}
