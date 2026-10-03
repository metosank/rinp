#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include <winsock2.h>

#include "job_queue.h"

class HttpServer {
public:
    using ActionHandler = std::function<bool(uint8_t)>;
    using ClipboardHandler = std::function<bool(std::string&)>;
    using JobHandler = std::function<bool(InputJob)>;
    using ExitHandler = std::function<void()>;

    HttpServer(
        std::string secretPath,
        ActionHandler actionHandler,
        ClipboardHandler clipboardHandler,
        JobHandler jobHandler,
        ExitHandler exitHandler
    );
    ~HttpServer();

    bool start(uint16_t port);
    void run();
    void stop();
    bool isRunning() const;

private:
    struct ClientState {
        std::string buffer;
        size_t expectedSize = 0;
        std::string pendingSend;
        size_t sentOffset = 0;
        DWORD lastActivity = 0;
    };

    void acceptClients();
    void handleReadable(SOCKET client, ClientState& state);
    void handleWritable(SOCKET client, ClientState& state);
    bool tryProcessRequest(SOCKET client, ClientState& state);
    void queueSend(SOCKET client, ClientState& state, std::string data);
    void removeClient(SOCKET client);

    void buildResponse(
        std::string& out,
        const std::string& status,
        const std::string& contentType,
        const std::string& body
    );
    void buildGzipResponse(std::string& out);

    static bool parseRequestLine(
        const std::string& header,
        std::string& method,
        std::string& target
    );
    static long long parseContentLength(const std::string& header);

    std::string secretPath_;
    ActionHandler actionHandler_;
    ClipboardHandler clipboardHandler_;
    JobHandler jobHandler_;
    ExitHandler exitHandler_;
    SOCKET listener_ = INVALID_SOCKET;
    std::atomic<bool> running_{false};
    std::unordered_map<SOCKET, ClientState> clients_;
};