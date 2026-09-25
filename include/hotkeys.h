#pragma once

#include <atomic>
#include <thread>

#include <winsock2.h>
#include <windows.h>

class AppController;

class HotkeyController {
public:
    explicit HotkeyController(AppController& app);
    void run();
    void stop();

private:
    bool registerHotkeys();
    void unregisterHotkeys();

    AppController& app_;
    std::atomic<DWORD> threadId_{0};
};
