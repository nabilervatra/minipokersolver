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
- `src/api_server.cpp`: local HTTP API server around C++ engine
- `ui/index.html`: clickable browser UI (human vs random)
- `ui/engine-api.js`: browser API client for `http://localhost:8080`
- `ui/app.js`: UI rendering + click handlers
- `ui/styles.css`: UI styling

## Build and run

### Option 1: CMake (if installed)

```bash
cmake -S . -B build
cmake --build build
./build/poker_solver
./build/poker_solve
```

### Option 2: Direct clang++

```bash
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/main.cpp src/poker_engine.cpp -o poker_solver
./poker_solver

clang++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/tree_builder.cpp src/solve_main.cpp -o poker_solve
./poker_solve
```

## Solver Scaffold (Step 1 + 2)

- `include/poker/tree.hpp`: strict betting abstraction + tree node/state types
- `src/tree_builder.cpp`: deterministic game tree generator
- `src/solve_main.cpp`: scaffold executable that builds the tree and prints node stats

## Clickable UI

Start the C++ API server (terminal 1):

```bash
./build/poker_api_server
```

Serve UI files (terminal 2):

```bash
python3 -m http.server 8000
```

Then open:

- `http://localhost:8000/ui/`

Notes:

- Uses card images from `asset/`.
- UI calls the C++ server API directly (no duplicated engine rules in JS).
- API endpoints:
  - `POST /new_hand`
  - `GET /state`
  - `GET /legal_actions`
  - `POST /apply_action` with JSON body `{\"index\": <number>}`
  - `POST /apply_random_action`
  - `GET /terminal_result`

## Current limitations

- Heads-up only
- No side pots
- No blinds/position abstraction beyond fixed 2-player setup
- Simplified betting flow suitable for Milestone A scaffolding
