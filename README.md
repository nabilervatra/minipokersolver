# Poker Solver (C++)

Milestone A boilerplate: a core heads-up no-limit poker engine without solving.

## Implemented

- `State` with:
  - `street`, `pot`, `stacks`, `to_act`, `bet_to_call`, `last_bet_size`
  - action history
  - per-round commitments, fold flags, cards
- Legal action generation with restricted sizes:
  - check / fold / call
  - bet and raise sizes: `0.5x pot`, `1.0x pot`, `2.0x pot`, and all-in
- Terminal payoff:
  - fold: remaining player wins pot
  - showdown: 7-card hand evaluation (best 5 of 7)
- Random simulation driver:
  - simulates multiple hands using random legal actions
  - guard against infinite loops

## Project layout

- `include/poker/types.hpp`: core state/action/result types
- `include/poker/engine.hpp`: engine API
- `src/poker_engine.cpp`: engine implementation
- `src/main.cpp`: simulation smoke test

## Build and run

### Option 1: CMake (if installed)

```bash
cmake -S . -B build
cmake --build build
./build/poker_solver
```

### Option 2: Direct clang++

```bash
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/main.cpp src/poker_engine.cpp -o poker_solver
./poker_solver
```

## Current limitations

- Heads-up only
- No side pots
- No blinds/position abstraction beyond fixed 2-player setup
- Simplified betting flow suitable for Milestone A scaffolding
