#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0500
#endif

#include <windows.h>
#include <shellapi.h>

class AppController;

class TrayController {
public:
    TrayController(AppController& app, uint16_t port, std::string secretPath);

    void run();
    void stop();
    void setVisible(bool visible);

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void showMenu(HWND hwnd);
    bool refreshLocalAddresses();
    bool addTrayIcon(HWND hwnd);
    void removeTrayIcon(HWND hwnd);
    void setTrayIconVisible(HWND hwnd, bool visible);

    AppController& app_;
    uint16_t port_;
    std::string secretPath_;
    std::vector<std::string> localAddresses_;
    std::atomic<DWORD> threadId_{0};
    std::atomic<bool> trayIconVisible_{false};
};
