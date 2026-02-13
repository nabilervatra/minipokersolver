function createPokerEngineApi(baseUrl = "http://localhost:8080") {
  async function request(path, options = {}) {
    const res = await fetch(`${baseUrl}${path}`, {
      headers: {
        "Content-Type": "application/json",
        ...(options.headers || {}),
      },
      ...options,
    });

    const text = await res.text();
    let data = null;
    if (text) {
      data = JSON.parse(text);
    }

    if (!res.ok) {
      const msg = data && data.error ? data.error : `HTTP ${res.status}`;
      throw new Error(msg);
    }
    return data;
  }

  return {
    async newHand() {
      return request("/new_hand", { method: "POST", body: "{}" });
    },
    async getState() {
      return request("/state");
    },
    async getLegalActions() {
      return request("/legal_actions");
    },
    async applyActionIndex(index) {
      const out = await request("/apply_action", {
        method: "POST",
        body: JSON.stringify({ index }),
      });
      return !!out.ok;
    },
    async applyRandomAction() {
      const out = await request("/apply_random_action", {
        method: "POST",
        body: "{}",
      });
      return !!out.ok;
    },
    async getTerminalResult() {
      return request("/terminal_result");
    },
    async health() {
      return request("/health");
    },
  };
}
