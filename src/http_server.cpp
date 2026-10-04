#include "http_server.h"

#include "web_page.h"
#include "win32_utils.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>
#include <optional>

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
    "Connection: close\r\n";

constexpr size_t kCorsHeadersSize = kCorsHeaders.size();

constexpr const char* statusLine(int code) noexcept {
    switch (code) {
        case 200: return "200 OK";
        case 204: return "204 No Content";
        case 301: return "301 Moved Permanently";
        case 400: return "400 Bad Request";
        case 404: return "404 Not Found";
        case 405: return "405 Method Not Allowed";
        case 500: return "500 Internal Server Error";
        case 503: return "503 Service Unavailable";
        default:  return "500 Internal Server Error";
    }
}

std::string buildResponse(
    std::string_view status,
    bool isNormalResponse,
    std::string_view contentType,
    std::string_view body
) {
    std::string out;

    size_t totalSize = 9 + status.size() + 18 + contentType.size() + 2 + body.size();
    if (isNormalResponse) {
        totalSize += kCorsHeadersSize + 2;
    }
    
    out.reserve(totalSize);
    out.append("HTTP/1.1 ");
    out.append(status);
    out.append("\r\nContent-Type: ");
    out.append(contentType);
    out.append("\r\n");
    if (isNormalResponse) {
        out.append(kCorsHeaders);
        out.append("\r\n");
    }
    out.append(body);
    return out;
}

std::string buildResponse(
    int statusCode,
    std::string_view contentType,
    std::string_view body
) {
    return buildResponse(statusLine(statusCode), statusCode < 300, contentType, body);
}


const std::string kIndexResponse = [] {
    std::string s =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Encoding: gzip\r\n\r\n";
    s.append(reinterpret_cast<const char*>(HTML_GZIP_DATA), HTML_GZIP_SIZE);
    return s;
}();

const auto kOKResponse = buildResponse(200, "text/plain; charset=utf-8", ".");
const auto kOPTIONSResponse = buildResponse(204, "text/plain; charset=utf-8", "");
const auto kHEADResponse = buildResponse(200, "text/html; charset=utf-8", "");
const auto kMETHOD_NOT_ALLOWED_Response = buildResponse(405, "text/plain; charset=utf-8", "405 Method Not Allowed");



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

    // 返回空字符串表示直接断开，返回nullopt表示继续等待数据，返回非空字符串表示发送响应
    auto res = tryParse(client, state);

    if (!res.has_value()) {
        return;
    }

    auto response = res.value();
    if(response.empty()) {
        removeClient(client);
        return;
    }

    queueSend(client, state, std::move(response));
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

std::optional<std::string> HttpServer::tryParse(SOCKET client, ClientState& state) {
    if (state.phase == ClientState::Phase::Header) {
        while (true) {
            size_t lineEnd = state.buffer.find("\r\n", state.parseOffset);
            if (lineEnd == std::string::npos) {
                if (state.buffer.size() > MAX_HEADER) return std::string();
                return std::nullopt;
            }

            size_t lineLen = lineEnd - state.parseOffset;

            // 解析到空行，表示请求头结束
            if (lineLen == 0) {
                if(!state.requestLineParsed) {
                    return std::string();
                }

                size_t bodyStart = lineEnd + 2;
                state.buffer.erase(0, bodyStart);
                state.parseOffset = 0;

                if (state.needsBody) {
                    if (state.contentLength < 0 || (unsigned long long)state.contentLength > MAX_BODY) {
                        return buildResponse(400, "text/plain; charset=utf-8", "content-length error");
                    }
                    state.phase = ClientState::Phase::Body;
                    break;
                }

                return handleRequest(client, state);
            }

            if (!state.requestLineParsed) {
                if (!parseRequestLine(state.buffer.data() + state.parseOffset, lineLen,
                                      state.method, state.target)) return std::string();
                state.requestLineParsed = true;
                if (!onRequestLineParsed(state)) return std::string();
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
        if (state.buffer.size() > MAX_BODY) return std::string();
        return std::nullopt;
    }

    return handleRequest(client, state);
}

bool HttpServer::onRequestLineParsed(ClientState& state) {
    if (state.target != secretPath_) return false;
    return true;
}

std::optional<std::string> HttpServer::handleRequest(SOCKET client, ClientState& state) {
    
    std::string response;

    if (state.method == "GET") {
        return kIndexResponse;
    }

    if (state.method == "OPTIONS") {
        return kOPTIONSResponse;
    }

    if (state.method == "HEAD") {
        return kHEADResponse;
    }

    if (state.method != "POST") {
        return kMETHOD_NOT_ALLOWED_Response;
    }

    const char* body = state.buffer.data();
    size_t bodyLength = state.buffer.size();

    if (bodyLength < 1) {
        return buildResponse(400, "text/plain; charset=utf-8", "empty body but need");
    }

    unsigned char firstByte = (unsigned char)body[0];

    if ((firstByte & 0x80) != 0) {
        if (bodyLength != 1) {
            return buildResponse(400, "text/plain; charset=utf-8", "expected 1 byte for action");
        }

        unsigned char action = firstByte & 0x7f;

        if (action == 0x7f) {
            std::string clipboardText;
            if (!clipboardHandler_ || !clipboardHandler_(clipboardText)) {
                return buildResponse(503, "text/plain; charset=utf-8",
                    "clipboard unavailable");
            }
            return buildResponse(200, "text/plain; charset=utf-8", clipboardText);
        }

        if (action == 1) {
            if(!exitHandler_) {
                return buildResponse(503, "text/plain; charset=utf-8",
                    "exit handler unavailable");
            }
            exitHandler_();
            return kOKResponse;
        }

        if (actionHandler_(action)) {
            return kOKResponse;
        }

        return buildResponse(400, "text/plain; charset=utf-8", "invalid action");

    }

    if (bodyLength < 2) {
        return buildResponse(400, "text/plain; charset=utf-8", "expected at least 2 bytes for input job");
    }

    InputJob job;
    job.delayMs = (uint16_t)((firstByte << 8) | (unsigned char)body[1]);
    if (!win32::utf8ToUtf16(body + 2, bodyLength - 2, job.text)) {
        return buildResponse(400, "text/plain; charset=utf-8", "invalid UTF-8");
    }
    if (!jobHandler_(std::move(job))) {
        return buildResponse(503, "text/plain; charset=utf-8",
            "server is stopping");
    }

    return kOKResponse;
}

void HttpServer::queueSend(SOCKET client, ClientState& state, std::string data) {
    state.pendingSend = std::move(data);
    state.sentOffset = 0;
    handleWritable(client, state);
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