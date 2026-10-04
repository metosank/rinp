#include "http_server.h"

#include "web_page.h"
#include "win32_utils.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

namespace {

constexpr size_t MAX_HEADER = 8 * 1024;
constexpr size_t MAX_BODY = 8 * 1024;
constexpr size_t MAX_CONNECTIONS = 1024;
constexpr DWORD IDLE_TIMEOUT_MS = 30000;

constexpr std::string_view kCorsHeaders =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET, HEAD, POST, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n"
    "Access-Control-Max-Age: 3600\r\n"
    "Connection: close\r\n\r\n";

constexpr size_t kCorsHeadersSize = kCorsHeaders.size();

} // namespace

HttpServer::HttpServer(
    std::string secretPath,
    ActionHandler actionHandler,
    ClipboardHandler clipboardHandler,
    JobHandler jobHandler,
    ExitHandler exitHandler
)
    : secretPath_(std::move(secretPath)),
      actionHandler_(std::move(actionHandler)),
      clipboardHandler_(std::move(clipboardHandler)),
      jobHandler_(std::move(jobHandler)),
      exitHandler_(std::move(exitHandler)) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start(uint16_t port) {
    listener_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener_ == INVALID_SOCKET) return false;

    int option = 1;
    setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, (const char*)&option, sizeof(option));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(listener_, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        stop();
        return false;
    }

    if (listen(listener_, SOMAXCONN) == SOCKET_ERROR) {
        stop();
        return false;
    }

    u_long nonBlocking = 1;
    ioctlsocket(listener_, FIONBIO, &nonBlocking);

    running_.store(true);
    return true;
}

void HttpServer::run() {
    std::vector<WSAPOLLFD> pollFds;
    pollFds.reserve(256);

    while (running_.load()) {
        pollFds.clear();
        pollFds.push_back({listener_, POLLIN, 0});

        for (auto& [sock, state] : clients_) {
            short events = POLLIN;
            if (!state.pendingSend.empty()) events = POLLOUT;
            pollFds.push_back({sock, events, 0});
        }

        int count = WSAPoll(pollFds.data(), (ULONG)pollFds.size(), 1000);
        if (count == SOCKET_ERROR) break;

        if (pollFds[0].revents & POLLIN) {
            acceptClients();
        }

        std::vector<SOCKET> toRemove;

        for (size_t i = 1; i < pollFds.size(); ++i) {
            SOCKET sock = pollFds[i].fd;
            short revents = pollFds[i].revents;
            auto it = clients_.find(sock);
            if (it == clients_.end()) continue;

            if (revents & (POLLHUP | POLLERR | POLLNVAL)) {
                toRemove.push_back(sock);
                continue;
            }

            if (revents & POLLOUT) {
                handleWritable(sock, it->second);
                it = clients_.find(sock);
                if (it == clients_.end()) continue;
            }

            if (revents & POLLIN) {
                handleReadable(sock, it->second);
            }
        }

        DWORD now = GetTickCount();
        for (auto& [sock, state] : clients_) {
            if (now - state.lastActivity > IDLE_TIMEOUT_MS) {
                toRemove.push_back(sock);
            }
        }

        for (SOCKET sock : toRemove) {
            removeClient(sock);
        }
    }

    for (auto& [sock, state] : clients_) {
        closesocket(sock);
    }
    clients_.clear();
}

void HttpServer::stop() {
    running_.store(false);
}

bool HttpServer::isRunning() const {
    return running_.load();
}

void HttpServer::acceptClients() {
    while (true) {
        SOCKET client = accept(listener_, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;

        if (clients_.size() >= MAX_CONNECTIONS) {
            closesocket(client);
            continue;
        }

        u_long nonBlocking = 1;
        ioctlsocket(client, FIONBIO, &nonBlocking);

        ClientState state;
        state.lastActivity = GetTickCount();
        clients_[client] = std::move(state);
    }
}

void HttpServer::removeClient(SOCKET client) {
    auto it = clients_.find(client);
    if (it != clients_.end()) {
        closesocket(client);
        clients_.erase(it);
    }
}

void HttpServer::handleReadable(SOCKET client, ClientState& state) {
    char buffer[8192];

    while (true) {
        int received = recv(client, buffer, sizeof(buffer), 0);
        if (received > 0) {
            state.buffer.append(buffer, (size_t)received);
            state.lastActivity = GetTickCount();
            continue;
        }
        if (received == 0) {
            removeClient(client);
            return;
        }
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) break;
        removeClient(client);
        return;
    }

    if (!tryParse(client, state)) {
        removeClient(client);
    }
}

void HttpServer::handleWritable(SOCKET client, ClientState& state) {
    size_t remaining = state.pendingSend.size() - state.sentOffset;
    int sent = send(client, state.pendingSend.data() + state.sentOffset, (int)remaining, 0);
    if (sent > 0) {
        state.sentOffset += (size_t)sent;
        state.lastActivity = GetTickCount();
        if (state.sentOffset >= state.pendingSend.size()) {
            removeClient(client);
        }
        return;
    }
    int error = WSAGetLastError();
    if (error != WSAEWOULDBLOCK) {
        removeClient(client);
    }
}

bool HttpServer::tryParse(SOCKET client, ClientState& state) {
    if (state.phase == ClientState::Phase::Header) {
        while (true) {
            size_t lineEnd = state.buffer.find("\r\n", state.parseOffset);
            if (lineEnd == std::string::npos) {
                if (state.buffer.size() > MAX_HEADER) return false;
                return true;
            }

            size_t lineLen = lineEnd - state.parseOffset;

            // 解析到空行，表示请求头结束
            if (lineLen == 0) {
                size_t bodyStart = lineEnd + 2;
                state.buffer.erase(0, bodyStart);
                state.parseOffset = 0;

                if (state.needsBody) {
                    if (state.contentLength < 0 || (unsigned long long)state.contentLength > MAX_BODY) {
                        std::string response;
                        buildResponse(response, "400 Bad Request", "text/plain; charset=utf-8", "400");
                        queueSend(client, state, std::move(response));
                        return true;
                    }
                    state.phase = ClientState::Phase::Body;
                    break;
                }

                return handleRequest(client, state);
            }

            if (!state.requestLineParsed) {
                if (!parseRequestLine(state.buffer.data() + state.parseOffset, lineLen,
                                      state.method, state.target)) return false;
                state.requestLineParsed = true;
                if (!onRequestLineParsed(state)) return false;
                state.needsBody = (state.method == "POST");
            } else if (state.needsBody && state.contentLength < 0) {
                long long cl = parseContentLengthLine(
                    state.buffer.data() + state.parseOffset, lineLen);
                if (cl >= 0) state.contentLength = cl;
            }

            state.parseOffset = lineEnd + 2;
        }
    }

    if (state.buffer.size() < (size_t)state.contentLength) {
        if (state.buffer.size() > MAX_BODY) return false;
        return true;
    }

    return handleRequest(client, state);
}

bool HttpServer::onRequestLineParsed(ClientState& state) {
    if (state.target != secretPath_) return false;
    return true;
}

bool HttpServer::handleRequest(SOCKET client, ClientState& state) {
    std::string response;

    if (state.method == "OPTIONS") {
        buildResponse(response, "204 No Content", "text/plain; charset=utf-8", "");
        queueSend(client, state, std::move(response));
        return true;
    }

    if (state.method == "HEAD") {
        buildResponse(response, "200 OK", "text/html; charset=utf-8", "");
        queueSend(client, state, std::move(response));
        return true;
    }

    if (state.method == "GET") {
        buildGzipResponse(response);
        queueSend(client, state, std::move(response));
        return true;
    }

    if (state.method != "POST") {
        buildResponse(response, "405 Method Not Allowed", "text/plain; charset=utf-8",
                      "405 Method Not Allowed");
        queueSend(client, state, std::move(response));
        return true;
    }

    const char* body = state.buffer.data();
    size_t bodyLength = state.buffer.size();

    if (bodyLength < 1) {
        buildResponse(response, "400 Bad Request", "text/plain; charset=utf-8", "400");
        queueSend(client, state, std::move(response));
        return true;
    }

    unsigned char firstByte = (unsigned char)body[0];

    if ((firstByte & 0x80) != 0) {
        if (bodyLength != 1) {
            buildResponse(response, "400 Bad Request", "text/plain; charset=utf-8", "400");
            queueSend(client, state, std::move(response));
            return true;
        }

        unsigned char action = firstByte & 0x7f;

        if (action == 0x7f) {
            std::string clipboardText;
            if (!clipboardHandler_ || !clipboardHandler_(clipboardText)) {
                buildResponse(response, "503 Service Unavailable", "text/plain; charset=utf-8",
                              "clipboard unavailable");
                queueSend(client, state, std::move(response));
                return true;
            }
            buildResponse(response, "200 OK", "text/plain; charset=utf-8", clipboardText);
            queueSend(client, state, std::move(response));
            return true;
        }

        if (action != 1 && !actionHandler_(action)) {
            buildResponse(response, "400 Bad Request", "text/plain; charset=utf-8", "invalid action");
            queueSend(client, state, std::move(response));
            return true;
        }

        buildResponse(response, "200 OK", "text/plain; charset=utf-8", ".");
        queueSend(client, state, std::move(response));
        if (action == 1 && exitHandler_) exitHandler_();
        return true;
    }

    if (bodyLength < 2) {
        buildResponse(response, "400 Bad Request", "text/plain; charset=utf-8", "400");
        queueSend(client, state, std::move(response));
        return true;
    }

    InputJob job;
    job.delayMs = (uint16_t)((firstByte << 8) | (unsigned char)body[1]);
    if (!win32::utf8ToUtf16(body + 2, bodyLength - 2, job.text)) {
        buildResponse(response, "400 Bad Request", "text/plain; charset=utf-8", "invalid UTF-8");
        queueSend(client, state, std::move(response));
        return true;
    }
    if (!jobHandler_(std::move(job))) {
        buildResponse(response, "503 Service Unavailable", "text/plain; charset=utf-8",
                      "server is stopping");
        queueSend(client, state, std::move(response));
        return true;
    }

    buildResponse(response, "200 OK", "text/plain; charset=utf-8", ".");
    queueSend(client, state, std::move(response));
    return true;
}

void HttpServer::queueSend(SOCKET client, ClientState& state, std::string data) {
    state.pendingSend = std::move(data);
    state.sentOffset = 0;
    handleWritable(client, state);
}

void HttpServer::buildResponse(
    std::string& out,
    const std::string& status,
    const std::string& contentType,
    const std::string& body
) {
    out.reserve(9+3+18 + 2+contentType.size() + kCorsHeadersSize + body.size());
    out.append("HTTP/1.1 ");
    out.append(status);
    out.append("\r\nContent-Type: ");
    out.append(contentType);
    out.append("\r\n");
    out.append(kCorsHeaders);
    out.append(body);
}

void HttpServer::buildGzipResponse(std::string& out) {
    static constexpr std::string_view kHeaders =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Encoding: gzip\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, HEAD, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Access-Control-Max-Age: 3600\r\n"
        "Connection: close\r\n\r\n";

    out.reserve(256 + HTML_GZIP_SIZE);
    out.append(kHeaders);
    out.append(reinterpret_cast<const char*>(HTML_GZIP_DATA), HTML_GZIP_SIZE);
}

bool HttpServer::parseRequestLine(
    const char* line,
    size_t len,
    std::string& method,
    std::string& target
) {
    const char* firstSpace = (const char*)std::memchr(line, ' ', len);
    if (!firstSpace) return false;
    size_t firstPos = (size_t)(firstSpace - line);

    const char* secondSpace = (const char*)std::memchr(
        firstSpace + 1, ' ', len - firstPos - 1);
    if (!secondSpace) return false;
    size_t secondPos = (size_t)(secondSpace - line);

    method.assign(line, firstPos);
    target.assign(line + firstPos + 1, secondPos - firstPos - 1);
    return true;
}

long long HttpServer::parseContentLengthLine(const char* line, size_t len) {
    static const char kField[] = "content-length:";
    constexpr size_t kFieldLen = sizeof(kField) - 1;

    if (len <= kFieldLen) return -1;

    for (size_t i = 0; i < kFieldLen; ++i) {
        if (std::tolower((unsigned char)line[i]) != kField[i]) return -1;
    }

    size_t pos = kFieldLen;
    while (pos < len && (line[pos] == ' ' || line[pos] == '\t')) ++pos;

    long long value = 0;
    bool hasValue = false;
    while (pos < len && std::isdigit((unsigned char)line[pos])) {
        value = value * 10 + (line[pos] - '0');
        hasValue = true;
        ++pos;
        if (value > 1000000000LL) break;
    }

    return hasValue ? value : -1;
}