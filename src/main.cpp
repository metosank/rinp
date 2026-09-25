#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <mutex>
#include <memory>
#include <utility>
#include <vector>

#include "win32_utils.h"
#include "app_controller.h"
#include "hotkeys.h"
#include "tray.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

#ifndef MB_ERR_INVALID_CHARS
#define MB_ERR_INVALID_CHARS 0x08000000
#endif

static std::string makeSecretPath() {
    static const char alphabet[] = "abcdefghijkmnpqrstuvwxyz23456789";
    static constexpr size_t alphabetLen = sizeof(alphabet) - 1;

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> dist(0, alphabetLen - 1);

    std::string path;
    path.reserve(4);
    path.push_back('/');

    for (int i = 0; i < 3; ++i) {
        path.push_back(alphabet[dist(rng)]);
    }

    return path;
}

static int makeRandomPort(int start, int end) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(start, end);
    return dist(rng);
}

static bool parsePortValue(const char* value, int& port) {
    if (value == nullptr || *value == '\0') {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    long parsed = std::strtol(value, &end, 10);
    if (errno == ERANGE || *end != '\0' || parsed < 1 || parsed > 65535) {
        return false;
    }

    port = (int)parsed;
    return true;
}

static bool isValidSecretPath(const char* path) {
    if (path == nullptr || *path == '\0') {
        return false;
    }

    for (const unsigned char* p = (const unsigned char*)path; *p != '\0'; ++p) {
        if (*p <= 0x20 || *p == 0x7F || *p == '/' || *p == '\\' || *p == '?' || *p == '#') {
            return false;
        }
    }

    return true;
}

static bool isExpired() {
    SYSTEMTIME localTime = {};
    GetLocalTime(&localTime);
    return localTime.wYear >= 2027;
}

static void printUsage(const char* program, const char* errorFormat = nullptr, ...) {
    std::string message;

    if (errorFormat != nullptr) {
        va_list args;
        va_start(args, errorFormat);
        message = win32::formatMessage(errorFormat, args);
        va_end(args);
        message += "\n";
    }

    message += "用法: ";
    message += program;
    message += " [-d] [--port PORT | --port-start PORT --port-end PORT] [--path PATH]\n";
    message += "  -d                           启动后删除自身文件并继续运行\n";
    message += "  --port PORT                  指定端口\n";
    message += "  --port-start PORT            随机端口范围起始值\n";
    message += "  --port-end PORT              随机端口范围结束值\n";
    message += "  --path PATH                  指定 HTTP 路径（不包含开头的 /）";

    win32::showError(message);
}

int main(int argc, char** argv) {
    win32::enableDpiAwareness();
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    if (isExpired()) {
        //win32::reportError("");
        return 1;
    }

    int port = 0;
    int portStart = 2000;
    int portEnd = 9000;
    int specifiedPort = 0;
    bool hasPortStart = false;
    bool hasPortEnd = false;
    bool hasSpecifiedPort = false;
    bool hasSpecifiedPath = false;
    bool deleteOnStart = false;
    std::string secretPath;

    for (int index = 1; index < argc; ++index) {
        const char* option = argv[index];
        const char* value = nullptr;

        if (std::strcmp(option, "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }

        if (std::strcmp(option, "-d") == 0) {
            if (deleteOnStart) {
                printUsage(argv[0], "选项重复: %s", option);
                return 1;
            }
            deleteOnStart = true;
            continue;
        }

        if (std::strcmp(option, "--port") == 0 ||
            std::strcmp(option, "--port-start") == 0 ||
            std::strcmp(option, "--port-end") == 0 ||
            std::strcmp(option, "--path") == 0) {
            if (index + 1 >= argc) {
                printUsage(argv[0], "选项缺少参数: %s", option);
                return 1;
            }
            value = argv[++index];
        } else {
            printUsage(argv[0], "未知选项: %s", option);
            return 1;
        }

        if (std::strcmp(option, "--port") == 0) {
            if (hasSpecifiedPort || !parsePortValue(value, specifiedPort)) {
                printUsage(argv[0], "无效的 --port: %s", value);
                return 1;
            }
            hasSpecifiedPort = true;
        } else if (std::strcmp(option, "--port-start") == 0) {
            if (hasPortStart || !parsePortValue(value, portStart)) {
                printUsage(argv[0], "无效的 --port-start: %s", value);
                return 1;
            }
            hasPortStart = true;
        } else if (std::strcmp(option, "--port-end") == 0) {
            if (hasPortEnd || !parsePortValue(value, portEnd)) {
                printUsage(argv[0], "无效的 --port-end: %s", value);
                return 1;
            }
            hasPortEnd = true;
        } else {
            if (hasSpecifiedPath || !isValidSecretPath(value)) {
                printUsage(argv[0], "无效的 --path: %s", value);
                return 1;
            }
            secretPath = "/";
            secretPath += value;
            hasSpecifiedPath = true;
        }
    }

    if (hasSpecifiedPort && (hasPortStart || hasPortEnd)) {
        printUsage(argv[0], "--port 不能与 --port-start 或 --port-end 同时使用");
        return 1;
    }

    if (hasPortStart != hasPortEnd || (hasPortStart && portStart > portEnd)) {
        printUsage(argv[0], "随机端口范围必须同时指定，且起始值不能大于结束值");
        return 1;
    }

    if (hasSpecifiedPort) {
        port = specifiedPort;
    } else {
        port = makeRandomPort(portStart, portEnd);
    }

    if (!hasSpecifiedPath) {
        secretPath = makeSecretPath();
    }

    HANDLE instanceMutex = CreateMutexW(
        nullptr,
        TRUE,
        L"Local\\rinp.SingleInstance"
    );
    if (instanceMutex == nullptr) {
        win32::showError("无法创建单实例锁定对象。");
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(instanceMutex);
        if(deleteOnStart){
            win32::deleteCurrentExecutable();
        }
        win32::showError("已在运行，请查看任务栏托盘\n若隐藏了托盘图标，请按快捷键 Ctrl+Shift+B 显示");
        return 1;
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        win32::reportError("初始化网络失败: %d", WSAGetLastError());
        CloseHandle(instanceMutex);
        return 1;
    }

    std::vector<std::string> localAddresses;
    AppController app(secretPath, (uint16_t)port);
    HotkeyController hotkeyController(app);
    TrayController trayController(app, (uint16_t)port, secretPath);
    app.setTrayVisibilityHandler([&trayController](bool visible) {
        trayController.setVisible(visible);
    });

    if (!app.start()) {
        win32::reportError("HTTP服务器启动失败: %d", WSAGetLastError());
        WSACleanup();
        CloseHandle(instanceMutex);
        return 1;
    }

    if (deleteOnStart) app.scheduleProgramFileDeletion();

    std::printf("快捷键:\n  Ctrl+Shift+Z 紧急退出\n  Ctrl+Shift+X 中止\n  Ctrl+Shift+C 暂停\n  Ctrl+Shift+V 恢复\n  Ctrl+Shift+B 显示托盘图标\n\n");
    std::printf("以下URL可用:\n");

    bool resolved = win32::resolveLocalAddresses(port, secretPath, localAddresses);
    if (!resolved) {
        std::printf("  **未发现可用地址**\n");
    }

    for (const std::string& address : localAddresses) {
        std::printf("  %s\n", address.c_str());
    }
    std::fflush(stdout);

    std::thread hotkeyThread([&hotkeyController] { hotkeyController.run(); });
    std::thread trayThread([&trayController] { trayController.run(); });

    app.run();
    app.requestExit();
    hotkeyController.stop();
    trayController.stop();
    if (hotkeyThread.joinable()) hotkeyThread.join();
    if (trayThread.joinable()) trayThread.join();
    WSACleanup();
    CloseHandle(instanceMutex);
    return 0;
}
