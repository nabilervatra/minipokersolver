#include "poker/engine.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

volatile std::sig_atomic_t g_stop = 0;

void on_sigint(int) {
    g_stop = 1;
}

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

std::string trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) {
        ++b;
    }
    std::size_t e = s.size();
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) {
        --e;
    }
    return s.substr(b, e - b);
}

int street_index(poker::Street s) {
    switch (s) {
        case poker::Street::Preflop:
            return 0;
        case poker::Street::Flop:
            return 1;
        case poker::Street::Turn:
            return 2;
        case poker::Street::River:
            return 3;
        case poker::Street::Showdown:
            return 4;
        case poker::Street::Terminal:
            return 5;
    }
    return -1;
}

std::string json_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (c == '\\') {
            out += "\\\\";
        } else if (c == '"') {
            out += "\\\"";
        } else {
            out += c;
        }
    }
    return out;
}

std::string action_to_json(const poker::Action& a) {
    std::ostringstream os;
    os << "{";
    os << "\"player\":" << a.player << ",";
    os << "\"type\":\"" << poker::to_string(a.type) << "\",";
    os << "\"amount\":" << a.amount << ",";
    os << "\"to_call_before\":" << a.to_call_before << ",";
    os << "\"street\":" << street_index(a.street);
    os << "}";
    return os.str();
}

std::string state_to_json(const poker::State& s) {
    std::ostringstream os;
    os << "{";
    os << "\"street\":" << street_index(s.street) << ",";
    os << "\"street_name\":\"" << poker::to_string(s.street) << "\",";
    os << "\"pot\":" << s.pot << ",";
    os << "\"stacks\":[" << s.stacks[0] << "," << s.stacks[1] << "],";
    os << "\"to_act\":" << s.to_act << ",";
    os << "\"bet_to_call\":" << s.bet_to_call << ",";
    os << "\"last_bet_size\":" << s.last_bet_size << ",";
    os << "\"committed_total\":[" << s.committed_total[0] << "," << s.committed_total[1] << "],";

    os << "\"hole_cards\":[";
    os << "[" << s.hole_cards[0][0] << "," << s.hole_cards[0][1] << "],";
    os << "[" << s.hole_cards[1][0] << "," << s.hole_cards[1][1] << "]";
    os << "],";

    os << "\"board\":[";
    for (std::size_t i = 0; i < s.board.size(); ++i) {
        if (i) {
            os << ",";
        }
        os << s.board[i];
    }
    os << "],";

    os << "\"history\":[";
    for (std::size_t i = 0; i < s.history.size(); ++i) {
        if (i) {
            os << ",";
        }
        os << action_to_json(s.history[i]);
    }
    os << "],";

    os << "\"is_terminal\":" << (s.street == poker::Street::Terminal ? "true" : "false");
    os << "}";
    return os.str();
}

std::string terminal_to_json(const poker::TerminalResult& r) {
    std::ostringstream os;
    os << "{";
    os << "\"is_terminal\":" << (r.is_terminal ? "true" : "false") << ",";
    os << "\"winner\":" << r.winner << ",";
    os << "\"reason\":\"" << json_escape(r.reason) << "\",";
    os << "\"chip_delta\":[" << r.chip_delta[0] << "," << r.chip_delta[1] << "]";
    os << "}";
    return os.str();
}

bool send_all(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

std::string status_text(int code) {
    switch (code) {
        case 200:
            return "OK";
        case 204:
            return "No Content";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        default:
            return "Internal Server Error";
    }
}

void send_json_response(int fd, int status_code, const std::string& body) {
    std::ostringstream os;
    os << "HTTP/1.1 " << status_code << ' ' << status_text(status_code) << "\r\n";
    os << "Content-Type: application/json\r\n";
    os << "Access-Control-Allow-Origin: *\r\n";
    os << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    os << "Access-Control-Allow-Headers: Content-Type\r\n";
    os << "Connection: close\r\n";
    os << "Content-Length: " << body.size() << "\r\n\r\n";
    os << body;
    (void)send_all(fd, os.str());
}

void send_empty_response(int fd, int status_code) {
    std::ostringstream os;
    os << "HTTP/1.1 " << status_code << ' ' << status_text(status_code) << "\r\n";
    os << "Access-Control-Allow-Origin: *\r\n";
    os << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    os << "Access-Control-Allow-Headers: Content-Type\r\n";
    os << "Connection: close\r\n";
    os << "Content-Length: 0\r\n\r\n";
    (void)send_all(fd, os.str());
}

bool parse_request(int fd, HttpRequest& req) {
    constexpr int kMaxRequestSize = 1 << 20;
    std::string buffer;
    buffer.reserve(4096);

    char tmp[4096];
    std::size_t header_end = std::string::npos;
    while (true) {
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) {
            return false;
        }
        buffer.append(tmp, static_cast<std::size_t>(n));
        if (buffer.size() > kMaxRequestSize) {
            return false;
        }
        header_end = buffer.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            break;
        }
    }

    std::string header_blob = buffer.substr(0, header_end);
    std::istringstream hs(header_blob);

    std::string request_line;
    if (!std::getline(hs, request_line)) {
        return false;
    }
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream rl(request_line);
    std::string version;
    if (!(rl >> req.method >> req.path >> version)) {
        return false;
    }

    std::string line;
    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto pos = line.find(':');
        if (pos == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, pos));
        const std::string value = trim(line.substr(pos + 1));
        req.headers[key] = value;
    }

    std::size_t content_length = 0;
    auto it = req.headers.find("Content-Length");
    if (it != req.headers.end()) {
        content_length = static_cast<std::size_t>(std::strtoul(it->second.c_str(), nullptr, 10));
    }

    const std::size_t body_start = header_end + 4;
    req.body = buffer.substr(body_start);
    while (req.body.size() < content_length) {
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) {
            return false;
        }
        req.body.append(tmp, static_cast<std::size_t>(n));
        if (req.body.size() > kMaxRequestSize) {
            return false;
        }
    }
    if (req.body.size() > content_length) {
        req.body.resize(content_length);
    }

    return true;
}

int parse_index_field(const std::string& body) {
    const auto key = body.find("\"index\"");
    if (key == std::string::npos) {
        return -1;
    }
    const auto colon = body.find(':', key);
    if (colon == std::string::npos) {
        return -1;
    }
    std::size_t i = colon + 1;
    while (i < body.size() && (body[i] == ' ' || body[i] == '\t')) {
        ++i;
    }
    bool neg = false;
    if (i < body.size() && body[i] == '-') {
        neg = true;
        ++i;
    }
    if (i >= body.size() || body[i] < '0' || body[i] > '9') {
        return -1;
    }
    int val = 0;
    while (i < body.size() && body[i] >= '0' && body[i] <= '9') {
        val = val * 10 + (body[i] - '0');
        ++i;
    }
    return neg ? -val : val;
}

} // namespace

int main() {
    std::signal(SIGINT, on_sigint);

    poker::Engine engine(1337);
    std::optional<poker::State> state = engine.new_hand();

    const int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return 1;
    }

    int opt = 1;
    (void)::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << "\n";
        ::close(server_fd);
        return 1;
    }

    if (::listen(server_fd, 16) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << "\n";
        ::close(server_fd);
        return 1;
    }

    std::cout << "Poker API listening on http://localhost:8080\n";
    while (!g_stop) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int client_fd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "accept failed: " << std::strerror(errno) << "\n";
            break;
        }

        HttpRequest req;
        if (!parse_request(client_fd, req)) {
            send_json_response(client_fd, 400, "{\"error\":\"invalid request\"}");
            ::close(client_fd);
            continue;
        }

        if (req.method == "OPTIONS") {
            send_empty_response(client_fd, 204);
            ::close(client_fd);
            continue;
        }

        if (!state.has_value()) {
            state = engine.new_hand();
        }

        if (req.method == "POST" && req.path == "/new_hand") {
            state = engine.new_hand();
            send_json_response(client_fd, 200, state_to_json(*state));
        } else if (req.method == "GET" && req.path == "/state") {
            send_json_response(client_fd, 200, state_to_json(*state));
        } else if (req.method == "GET" && req.path == "/legal_actions") {
            const auto legals = engine.legal_actions(*state);
            std::ostringstream os;
            os << "[";
            for (std::size_t i = 0; i < legals.size(); ++i) {
                if (i) {
                    os << ",";
                }
                os << action_to_json(legals[i]);
            }
            os << "]";
            send_json_response(client_fd, 200, os.str());
        } else if (req.method == "POST" && req.path == "/apply_action") {
            const int index = parse_index_field(req.body);
            const auto legals = engine.legal_actions(*state);
            if (index < 0 || index >= static_cast<int>(legals.size())) {
                send_json_response(client_fd, 400, "{\"ok\":false,\"error\":\"invalid index\"}");
            } else {
                const bool ok = engine.apply_action(*state, legals[static_cast<std::size_t>(index)]);
                send_json_response(client_fd, 200, std::string("{\"ok\":") + (ok ? "true" : "false") + "}");
            }
        } else if (req.method == "POST" && req.path == "/apply_random_action") {
            const auto legals = engine.legal_actions(*state);
            if (legals.empty()) {
                send_json_response(client_fd, 400, "{\"ok\":false,\"error\":\"no legal actions\"}");
            } else {
                const auto a = engine.random_legal_action(*state);
                const bool ok = engine.apply_action(*state, a);
                send_json_response(client_fd, 200, std::string("{\"ok\":") + (ok ? "true" : "false") + "}");
            }
        } else if (req.method == "GET" && req.path == "/terminal_result") {
            send_json_response(client_fd, 200, terminal_to_json(engine.terminal_payoff(*state)));
        } else if (req.method == "GET" && req.path == "/health") {
            send_json_response(client_fd, 200, "{\"ok\":true}");
        } else {
            send_json_response(client_fd, 404, "{\"error\":\"not found\"}");
        }

        ::close(client_fd);
    }

    ::close(server_fd);
    return 0;
}
