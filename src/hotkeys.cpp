#include "hotkeys.h"

#include "app_controller.h"
#include "win32_utils.h"

#include <windows.h>

HotkeyController::HotkeyController(AppController& app)
    : app_(app) {
}

bool HotkeyController::registerHotkeys() {
    if (!RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'Z')) {
        win32::reportError("注册快捷键 Ctrl+Shift+Z 失败: %lu", GetLastError());
        return false;
    }
    if (!RegisterHotKey(nullptr, 2, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'X')) {
        win32::reportError("注册快捷键 Ctrl+Shift+X 失败: %lu", GetLastError());
        unregisterHotkeys();
        return false;
    }
    if (!RegisterHotKey(nullptr, 3, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'C')) {
        win32::reportError("注册快捷键 Ctrl+Shift+C 失败: %lu", GetLastError());
        unregisterHotkeys();
        return false;
    }
    if (!RegisterHotKey(nullptr, 4, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'V')) {
        win32::reportError("注册快捷键 Ctrl+Shift+V 失败: %lu", GetLastError());
        unregisterHotkeys();
        return false;
    }
    if (!RegisterHotKey(nullptr, 5, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'B')) {
        win32::reportError("注册快捷键 Ctrl+Shift+B 失败: %lu", GetLastError());
        unregisterHotkeys();
        return false;
    }
    return true;
}

void HotkeyController::unregisterHotkeys() {
    UnregisterHotKey(nullptr, 1);
    UnregisterHotKey(nullptr, 2);
    UnregisterHotKey(nullptr, 3);
    UnregisterHotKey(nullptr, 4);
    UnregisterHotKey(nullptr, 5);
}

void HotkeyController::run() {
    threadId_.store(GetCurrentThreadId());
    if (!registerHotkeys()) return;

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message != WM_HOTKEY) continue;

        switch (message.wParam) {
        case 1:
            app_.emergencyExit();
            break;
        case 2:
            app_.abortInput();
            break;
        case 3:
            app_.pauseInput();
            break;
        case 4:
            app_.resumeInput();
            break;
        case 5:
            app_.showTrayIcon();
            break;
        default:
            break;
        }
    }

    unregisterHotkeys();
    threadId_.store(0);
}

void HotkeyController::stop() {
    DWORD threadId = threadId_.load();
    if (threadId != 0) PostThreadMessageW(threadId, WM_QUIT, 0, 0);
}
