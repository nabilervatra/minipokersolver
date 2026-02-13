const SUITS = ["spades", "hearts", "diamonds", "clubs"];
const RANKS = ["2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"];
const STREETS = ["Preflop", "Flop", "Turn", "River", "Showdown", "Terminal"];

const ui = {
  humanPlayer: document.getElementById("human-player"),
  newHand: document.getElementById("new-hand"),
  status: document.getElementById("status"),
  board: document.getElementById("board"),
  actionButtons: document.getElementById("action-buttons"),
  prompt: document.getElementById("prompt"),
  nextHand: document.getElementById("next-hand"),
  historyLog: document.getElementById("history-log"),
  player0Meta: document.getElementById("player-0-meta"),
  player1Meta: document.getElementById("player-1-meta"),
  player0Cards: document.getElementById("player-0-cards"),
  player1Cards: document.getElementById("player-1-cards"),
  panel0: document.getElementById("player-0-panel"),
  panel1: document.getElementById("player-1-panel"),
};

const game = {
  api: null,
  state: null,
  aiTimer: null,
  busy: false,
};

function rankOf(card) {
  return (card % 13) + 2;
}

function suitOf(card) {
  return Math.floor(card / 13);
}

function cardToFile(card) {
  const suit = SUITS[suitOf(card)];
  const rank = RANKS[rankOf(card) - 2];
  return `../asset/${suit}_${rank}.png`;
}

function createBackCard() {
  const div = document.createElement("div");
  div.className = "card back";
  div.textContent = "?";
  return div;
}

function createCardImg(card) {
  const img = document.createElement("img");
  img.className = "card";
  img.src = cardToFile(card);
  img.alt = `card-${card}`;
  return img;
}

function clearElement(el) {
  while (el.firstChild) el.removeChild(el.firstChild);
}

function renderCards(container, cards, hidden = false) {
  clearElement(container);
  cards.forEach((c) => {
    container.appendChild(hidden ? createBackCard() : createCardImg(c));
  });
}

function renderBoard(state) {
  clearElement(ui.board);
  state.board.forEach((c) => ui.board.appendChild(createCardImg(c)));
}

function renderMeta(state) {
  const start0 = state.stacks[0] + state.committed_total[0];
  const start1 = state.stacks[1] + state.committed_total[1];
  ui.player0Meta.textContent = `stack=${state.stacks[0]} | committed=${state.committed_total[0]} | start=${start0}`;
  ui.player1Meta.textContent = `stack=${state.stacks[1]} | committed=${state.committed_total[1]} | start=${start1}`;

  ui.panel0.style.outline = state.to_act === 0 && state.street !== 5 ? "2px solid #267f59" : "none";
  ui.panel1.style.outline = state.to_act === 1 && state.street !== 5 ? "2px solid #267f59" : "none";
}

function actionText(action) {
  return `P${action.player} ${action.type} amount=${action.amount} to_call_before=${action.to_call_before}`;
}

function renderHistory(state, result) {
  const lines = state.history.map((a, i) => `${i + 1}. [${STREETS[a.street]}] ${actionText(a)}`);
  if (result && result.is_terminal) {
    lines.push("-");
    lines.push(`Result: reason=${result.reason}, winner=${result.winner}, chip_delta=[${result.chip_delta[0]}, ${result.chip_delta[1]}]`);
  }
  ui.historyLog.textContent = lines.join("\n");
}

async function renderActionButtons(state) {
  clearElement(ui.actionButtons);
  ui.prompt.textContent = "";
  ui.nextHand.style.display = "none";

  if (state.street === 5) {
    ui.prompt.textContent = "Hand finished. Click New Hand for the next one.";
    ui.nextHand.style.display = "inline-block";
    return;
  }

  const human = Number(ui.humanPlayer.value);
  if (state.to_act !== human) {
    ui.prompt.textContent = "Opponent is thinking...";
    return;
  }

  const legals = await game.api.getLegalActions();
  legals.forEach((a, idx) => {
    const btn = document.createElement("button");
    btn.textContent = `${a.type} (${a.amount})`;
    btn.addEventListener("click", async () => {
      if (game.busy) return;
      game.busy = true;
      try {
        await game.api.applyActionIndex(idx);
        game.state = await game.api.getState();
      } finally {
        game.busy = false;
      }
      await render();
    });
    ui.actionButtons.appendChild(btn);
  });
}

function scheduleAiIfNeeded() {
  const state = game.state;
  if (!state || state.street === 5) return;

  const human = Number(ui.humanPlayer.value);
  if (state.to_act === human) return;
  if (game.aiTimer) return;

  game.aiTimer = setTimeout(async () => {
    game.aiTimer = null;
    if (game.busy) {
      scheduleAiIfNeeded();
      return;
    }
    game.busy = true;
    try {
      await game.api.applyRandomAction();
      game.state = await game.api.getState();
    } finally {
      game.busy = false;
    }
    await render();
  }, 350);
}

async function render() {
  const state = game.state;
  if (!state) return;

  const result = await game.api.getTerminalResult();
  const human = Number(ui.humanPlayer.value);

  ui.status.textContent = `Street=${STREETS[state.street]} | Pot=${state.pot} | To act=P${state.to_act} | Bet to call=${state.bet_to_call} | Last bet size=${state.last_bet_size}`;

  renderBoard(state);
  renderMeta(state);
  renderCards(ui.player0Cards, state.hole_cards[0], state.street !== 5 && human !== 0);
  renderCards(ui.player1Cards, state.hole_cards[1], state.street !== 5 && human !== 1);
  await renderActionButtons(state);
  renderHistory(state, result);
  scheduleAiIfNeeded();
}

async function startNewHand() {
  if (game.aiTimer) {
    clearTimeout(game.aiTimer);
    game.aiTimer = null;
  }
  game.state = await game.api.newHand();
  await render();
}

async function boot() {
  try {
    game.api = createPokerEngineApi("http://localhost:8080");
    await game.api.health();

    ui.newHand.addEventListener("click", () => {
      startNewHand();
    });
    ui.nextHand.addEventListener("click", () => {
      startNewHand();
    });
    ui.humanPlayer.addEventListener("change", () => {
      startNewHand();
    });

    await startNewHand();
  } catch (err) {
    ui.status.textContent = `Failed to connect to C++ API server: ${err.message}`;
    ui.prompt.textContent = "Run poker_api_server on localhost:8080, then refresh.";
  }
}

boot();
