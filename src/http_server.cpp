#include "http_server.h"

#include "web_page.h"
#include "win32_utils.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <thread>

namespace {

constexpr size_t MAX_HEADER = 64 * 1024;
constexpr size_t MAX_BODY = 16 * 1024 * 1024;

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

    return true;
}

void HttpServer::run() {
    while (isRunning()) {
        SOCKET listener = listener_;
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!isRunning()) return;
            continue;
        }

        int timeout = 30000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.push_back(client);
            clientThreads_.emplace_back(&HttpServer::handleClient, this, client);
        }
    }
}

void HttpServer::stop() {
    if (listener_ != INVALID_SOCKET) {
        closesocket(listener_);
        listener_ = INVALID_SOCKET;
    }

    std::vector<SOCKET> clients;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients = clients_;
    }
    for (SOCKET client : clients) {
        shutdown(client, SD_BOTH);
    }

    const auto currentThread = std::this_thread::get_id();
    std::vector<std::thread> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& thread : clientThreads_) {
            if (thread.joinable() && thread.get_id() != currentThread) {
                threadsToJoin.push_back(std::move(thread));
            }
        }
    }

    for (auto& thread : threadsToJoin) {
        thread.join();
    }

    std::lock_guard<std::mutex> lock(clientsMutex_);
    clientThreads_.erase(
        std::remove_if(
            clientThreads_.begin(),
            clientThreads_.end(),
            [](const std::thread& thread) { return !thread.joinable(); }
        ),
        clientThreads_.end()
    );
}

bool HttpServer::isRunning() const {
    return listener_ != INVALID_SOCKET;
}

void HttpServer::removeClient(SOCKET client) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto iterator = std::find(clients_.begin(), clients_.end(), client);
    if (iterator != clients_.end()) clients_.erase(iterator);
}

bool HttpServer::sendAll(SOCKET client, const char* data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        int count = send(client, data + sent, (int)(length - sent), 0);
        if (count <= 0) return false;
        sent += (size_t)count;
    }
    return true;
}

void HttpServer::sendResponse(
    SOCKET client,
    const std::string& status,
    const std::string& contentType,
    const std::string& body
) {
    static constexpr std::string_view kCorsHeaders = 
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, HEAD, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Access-Control-Max-Age: 3600\r\n"
        "Connection: close\r\n\r\n";

    size_t totalSize = 9 + status.size() + 2 +      // "HTTP/1.1 " + status + "\r\n"
        14 + contentType.size() + 2 + // "Content-Type: " + ct + "\r\n"
        kCorsHeaders.size() +
        body.size();

    std::string response;
    response.reserve(totalSize);

    response.append("HTTP/1.1 ");
    response.append(status);
    response.append("\r\nContent-Type: ");
    response.append(contentType);
    response.append("\r\n");
    //response.append("Content-Length: ").append(std::to_string(body.size())).append("\r\n");
    response.append(kCorsHeaders);
    response.append(body);

    sendAll(client, response.data(), response.size());
}

void HttpServer::sendGzipHtml(
    SOCKET client
) {
    static constexpr std::string_view kHeaders = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Encoding: gzip\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, HEAD, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Access-Control-Max-Age: 3600\r\n"
        "Connection: close\r\n\r\n";

    size_t totalSize = kHeaders.size() + HTML_GZIP_SIZE;

    std::string response;
    response.reserve(totalSize);

    response.append(kHeaders);
    response.append(reinterpret_cast<const char*>(HTML_GZIP_DATA), HTML_GZIP_SIZE);

    sendAll(client, response.data(), response.size());
}

bool HttpServer::parseRequestLine(
    const std::string& header,
    std::string& method,
    std::string& target
) {
    size_t firstSpace = header.find(' ');
    if (firstSpace == std::string::npos) return false;
    size_t secondSpace = header.find(' ', firstSpace + 1);
    if (secondSpace == std::string::npos) return false;

    method = header.substr(0, firstSpace);
    target = header.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    return true;
}

long long HttpServer::parseContentLength(const std::string& header) {
    std::string lowerHeader = header;
    std::transform(
        lowerHeader.begin(),
        lowerHeader.end(),
        lowerHeader.begin(),
        [](unsigned char character) { return (char)std::tolower(character); }
    );

    const char* key = "content-length:";
    size_t position = lowerHeader.find(key);
    if (position == std::string::npos) return -1;
    position += std::strlen(key);

    while (
        position < lowerHeader.size() &&
        (lowerHeader[position] == ' ' || lowerHeader[position] == '\t')
    ) {
        ++position;
    }

    long long value = 0;
    bool hasValue = false;
    while (position < lowerHeader.size() && std::isdigit((unsigned char)lowerHeader[position])) {
        value = value * 10 + (lowerHeader[position] - '0');
        hasValue = true;
        ++position;
        if (value > 1000000000LL) break;
    }
    return hasValue ? value : -1;
}

void HttpServer::handleClient(SOCKET client) {
    struct ClientCleanup {
        HttpServer* server;
        SOCKET client;

        ~ClientCleanup() {
            closesocket(client);
            server->removeClient(client);
        }
    } cleanup{this, client};

    std::string data;
    char buffer[8192];

    while (true) {
        int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            return;
        }
        data.append(buffer, (size_t)received);

        size_t headerEnd = data.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            if (data.size() > MAX_HEADER) return;
            continue;
        }

        std::string header = data.substr(0, headerEnd);
        std::string method;
        std::string target;
        if (!parseRequestLine(header, method, target)) {
            return;
        }
        if (target != secretPath_) {
            return;
        }

        if (method == "OPTIONS") {
            sendResponse(client, "204 No Content", "text/plain; charset=utf-8", "");
            return;
        }
        if (method == "HEAD") {
            sendResponse(client, "200 OK", "text/html; charset=utf-8", "");
            return;
        }
        if (method == "GET") {
            sendGzipHtml(client);
            return;
        }
        if (method != "POST") {
            sendResponse(client, "405 Method Not Allowed", "text/plain; charset=utf-8", "405 Method Not Allowed");
            return;
        }

        std::string lowerHeader = header;
        std::transform(
            lowerHeader.begin(),
            lowerHeader.end(),
            lowerHeader.begin(),
            [](unsigned char character) { return (char)std::tolower(character); }
        );
        if (
            lowerHeader.find("expect:") != std::string::npos &&
            lowerHeader.find("100-continue") != std::string::npos
        ) {
            static const char continueResponse[] = "HTTP/1.1 100 Continue\r\n\r\n";
            if (!sendAll(client, continueResponse, sizeof(continueResponse) - 1)) {
                return;
            }
        }

        long long contentLength = parseContentLength(header);
        if (contentLength < 0) {
            sendResponse(client, "400 Bad Request", "text/plain; charset=utf-8", "400");
            return;
        }
        if ((unsigned long long)contentLength > MAX_BODY) {
            sendResponse(client, "413 Payload Too Large", "text/plain; charset=utf-8", "413");
            return;
        }

        size_t bodyStart = headerEnd + 4;
        size_t expectedSize = bodyStart + (size_t)contentLength;
        if (data.size() < expectedSize) data.reserve(expectedSize);
        while (data.size() < expectedSize) {
            received = recv(client, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                return;
            }
            data.append(buffer, (size_t)received);
        }

        const char* body = data.data() + bodyStart;
        size_t bodyLength = (size_t)contentLength;
        if (bodyLength < 1) {
            sendResponse(client, "400 Bad Request", "text/plain; charset=utf-8", "400");
            return;
        }

        unsigned char firstByte = (unsigned char)body[0];
        if ((firstByte & 0x80) != 0) {
            if (bodyLength != 1) {
                sendResponse(client, "400 Bad Request", "text/plain; charset=utf-8", "400");
                return;
            }

            unsigned char action = firstByte & 0x7f;
            if (action == 0x7f) {
                std::string clipboardText;
                if (!clipboardHandler_ || !clipboardHandler_(clipboardText)) {
                    sendResponse(client, "503 Service Unavailable", "text/plain; charset=utf-8", "clipboard unavailable");
                    return;
                }
                sendResponse(client, "200 OK", "text/plain; charset=utf-8", clipboardText);
                return;
            }

            if (action != 1 && !actionHandler_(action)) {
                sendResponse(client, "400 Bad Request", "text/plain; charset=utf-8", "invalid action");
                return;
            }

            sendResponse(client, "200 OK", "text/plain; charset=utf-8", ".");
            if (action == 1 && exitHandler_) exitHandler_();
            return;
        }

        if (bodyLength < 2) {
            sendResponse(client, "400 Bad Request", "text/plain; charset=utf-8", "400");
            return;
        }

        InputJob job;
        job.delayMs = (uint16_t)((firstByte << 8) | (unsigned char)body[1]);
        if (!win32::utf8ToUtf16(body + 2, bodyLength - 2, job.text)) {
            sendResponse(client, "400 Bad Request", "text/plain; charset=utf-8", "invalid UTF-8");
            return;
        }
        if (!jobHandler_(std::move(job))) {
            sendResponse(client, "503 Service Unavailable", "text/plain; charset=utf-8", "server is stopping");
            return;
        }

        sendResponse(client, "200 OK", "text/plain; charset=utf-8", ".");
        return;
    }
}
