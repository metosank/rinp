#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "http_server.h"
#include "input_engine.h"
#include "job_queue.h"

class AppController {
public:
    using TrayVisibilityHandler = std::function<void(bool)>;

    AppController(std::string secretPath, uint16_t port);
    ~AppController();

    bool start();
    void run();
    void requestExit();
    void emergencyExit();

    void pauseInput();
    void resumeInput();
    void abortInput();
    void setRandomDelayEnabled(bool enabled);
    void setUnicodeDelayDoubleEnabled(bool enabled);
    void setTrayVisibilityHandler(TrayVisibilityHandler handler);
    void hideTrayIcon();
    void showTrayIcon();
    bool deleteProgramFile();
    void scheduleProgramFileDeletion();
    bool isProgramFileDeleted() const;

    bool isExitRequested() const;
    bool isInputEnabled() const;
    bool isRandomDelayEnabled() const;
    bool isUnicodeDelayDoubleEnabled() const;
    const std::string& secretPath() const;

private:
    bool handleHttpAction(uint8_t action);
    bool enqueueHttpJob(InputJob job);
    void workerLoop();

    std::string secretPath_;
    uint16_t port_;
    std::atomic<bool> exitRequested_{false};
    std::atomic<bool> programFileDeleted_{false};
    std::mutex programFileMutex_;
    std::thread programFileDeletionThread_;
    InputEngine inputEngine_;
    JobQueue jobQueue_;
    std::unique_ptr<HttpServer> httpServer_;
    std::thread workerThread_;
    TrayVisibilityHandler trayVisibilityHandler_;
};
