#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
    void handleClient(SOCKET client);
    void removeClient(SOCKET client);
    bool sendAll(SOCKET client, const char* data, size_t length);
    void sendResponse(
        SOCKET client,
        const std::string& status,
        const std::string& contentType,
        const std::string& body
    );
    
    void sendGzipHtml(
        SOCKET client
    );

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
    std::mutex clientsMutex_;
    std::vector<SOCKET> clients_;
    std::vector<std::thread> clientThreads_;
};
