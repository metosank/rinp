#include "tray.h"

#include "app_controller.h"
#include "res/resource.h"
#include "win32_utils.h"

#include <cstring>

namespace {
constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_TRAY_SET_VISIBILITY = WM_APP + 2;
constexpr UINT TRAY_COMMAND_EXIT = 1001;
constexpr UINT TRAY_COMMAND_COPY_ADDRESS_BASE = 1100;
constexpr UINT TRAY_COMMAND_EMERGENCY_EXIT = 1201;
constexpr UINT TRAY_COMMAND_ABORT = 1202;
constexpr UINT TRAY_COMMAND_PAUSE = 1203;
constexpr UINT TRAY_COMMAND_RESUME = 1204;
constexpr UINT TRAY_COMMAND_RANDOM_DELAY_OFF = 1205;
constexpr UINT TRAY_COMMAND_RANDOM_DELAY_ON = 1206;
constexpr UINT TRAY_COMMAND_DOUBLE_DELAY_OFF = 1207;
constexpr UINT TRAY_COMMAND_DOUBLE_DELAY_ON = 1208;
constexpr UINT TRAY_COMMAND_INPUT_ON = 1209;
constexpr UINT TRAY_COMMAND_INPUT_OFF = 1210;
constexpr UINT TRAY_COMMAND_ABOUT = 1211;
constexpr UINT TRAY_COMMAND_HIDE_TRAY = 1212;
constexpr UINT TRAY_COMMAND_SHOW_TRAY = 1213;
constexpr UINT TRAY_COMMAND_DELETE_PROGRAM_FILE = 1214;

void showAboutDialog(HWND hwnd) {
    const wchar_t* title = L"关于 rinp";
    const wchar_t* text =
        L"rinp\n"
        L"一个快捷输入工具\n\n"

        L"作者: metosank\n"
        L"GitHub: https://github.com/metosank/rinp\n"
    ;

    MessageBoxW(hwnd, text, title, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}
}

TrayController::TrayController(
    AppController& app,
    uint16_t port,
    std::string secretPath
)
    : app_(app),
      port_(port),
      secretPath_(std::move(secretPath)) {
}

LRESULT CALLBACK TrayController::windowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    auto* controller = reinterpret_cast<TrayController*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        controller = static_cast<TrayController*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(controller));
    }

    if (controller != nullptr) {
        return controller->handleMessage(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT TrayController::handleMessage(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    if (message == WM_COMMAND) {
        UINT command = LOWORD(wParam);
        if (command >= TRAY_COMMAND_COPY_ADDRESS_BASE &&
            command < TRAY_COMMAND_COPY_ADDRESS_BASE + localAddresses_.size()) {
            size_t index = command - TRAY_COMMAND_COPY_ADDRESS_BASE;
            win32::copyTextToClipboard(hwnd, localAddresses_[index]);
            return 0;
        }

        switch (command) {
        case TRAY_COMMAND_EMERGENCY_EXIT:
            app_.emergencyExit();
            return 0;
        case TRAY_COMMAND_ABORT:
            app_.abortInput();
            return 0;
        case TRAY_COMMAND_PAUSE:
        case TRAY_COMMAND_INPUT_OFF:
            app_.pauseInput();
            return 0;
        case TRAY_COMMAND_RESUME:
        case TRAY_COMMAND_INPUT_ON:
            app_.resumeInput();
            return 0;
        case TRAY_COMMAND_RANDOM_DELAY_OFF:
            app_.setRandomDelayEnabled(false);
            return 0;
        case TRAY_COMMAND_RANDOM_DELAY_ON:
            app_.setRandomDelayEnabled(true);
            return 0;
        case TRAY_COMMAND_DOUBLE_DELAY_OFF:
            app_.setUnicodeDelayDoubleEnabled(false);
            return 0;
        case TRAY_COMMAND_DOUBLE_DELAY_ON:
            app_.setUnicodeDelayDoubleEnabled(true);
            return 0;
        case TRAY_COMMAND_ABOUT:
            showAboutDialog(hwnd);
            return 0;
        case TRAY_COMMAND_HIDE_TRAY:
            app_.hideTrayIcon();
            return 0;
        case TRAY_COMMAND_SHOW_TRAY:
            app_.showTrayIcon();
            return 0;
        case TRAY_COMMAND_DELETE_PROGRAM_FILE:
            app_.deleteProgramFile();
            return 0;
        case TRAY_COMMAND_EXIT:
            app_.requestExit();
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
    }

    if (message == WM_DESTROY) {
        removeTrayIcon(hwnd);
        trayIconVisible_.store(false);
        PostQuitMessage(0);
        return 0;
    }

    if (message == WM_TRAYICON &&
        (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)) {
        showMenu(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void TrayController::showMenu(HWND hwnd) {
    bool hasValidAddresses = refreshLocalAddresses();

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) return;

    AppendMenuW(menu, MF_DISABLED | MF_STRING, 0, L"快捷键  点击执行");
    AppendMenuW(menu, MF_STRING, TRAY_COMMAND_EMERGENCY_EXIT, L"Ctrl+Shift+Z  紧急退出");
    AppendMenuW(menu, MF_STRING, TRAY_COMMAND_ABORT, L"Ctrl+Shift+X  中止");
    AppendMenuW(menu, MF_STRING, TRAY_COMMAND_PAUSE, L"Ctrl+Shift+C  暂停");
    AppendMenuW(menu, MF_STRING, TRAY_COMMAND_RESUME, L"Ctrl+Shift+V  恢复");
    AppendMenuW(menu, MF_STRING, TRAY_COMMAND_SHOW_TRAY, L"Ctrl+Shift+B  显示托盘图标");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_DISABLED | MF_STRING, 0, L"状态  点击切换");
    AppendMenuW(
        menu,
        MF_STRING,
        app_.isInputEnabled() ? TRAY_COMMAND_INPUT_OFF : TRAY_COMMAND_INPUT_ON,
        app_.isInputEnabled() ? L"输入状态: 已启用" : L"输入状态: 已暂停"
    );
    AppendMenuW(
        menu,
        MF_STRING,
        app_.isRandomDelayEnabled() ? TRAY_COMMAND_RANDOM_DELAY_OFF : TRAY_COMMAND_RANDOM_DELAY_ON,
        app_.isRandomDelayEnabled() ? L"随机延迟: 已开启" : L"随机延迟: 已关闭"
    );
    AppendMenuW(
        menu,
        MF_STRING,
        app_.isUnicodeDelayDoubleEnabled() ? TRAY_COMMAND_DOUBLE_DELAY_OFF : TRAY_COMMAND_DOUBLE_DELAY_ON,
        app_.isUnicodeDelayDoubleEnabled() ? L"增倍延迟: 已开启" : L"增倍延迟: 已关闭"
    );
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_DISABLED | MF_STRING, 0, L"可用URL  点击复制 ");
    if(!hasValidAddresses) {
        AppendMenuW(menu, MF_GRAYED | MF_STRING, 0, L"[未发现可用地址]");
    }
    for (size_t index = 0; index < localAddresses_.size(); ++index) {
        std::wstring address;
        if (!win32::utf8ToUtf16(
            localAddresses_[index].c_str(),
            localAddresses_[index].size(),
            address
        )) {
            continue;
        }
        AppendMenuW(
            menu,
            MF_STRING,
            TRAY_COMMAND_COPY_ADDRESS_BASE + index,
            address.c_str()
        );
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, TRAY_COMMAND_ABOUT, L"关于");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, TRAY_COMMAND_HIDE_TRAY, L"隐藏托盘图标");
    bool programFileDeleted = app_.isProgramFileDeleted();
    AppendMenuW(
        menu,
        programFileDeleted ? MF_GRAYED : MF_STRING,
        TRAY_COMMAND_DELETE_PROGRAM_FILE,
        programFileDeleted ? L"文件已删除" : L"删除程序文件"
    );
    AppendMenuW(menu, MF_STRING, TRAY_COMMAND_EXIT, L"退出");

    POINT point = {};
    GetCursorPos(&point);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
    PostMessageA(hwnd, WM_NULL, 0, 0);
}

bool TrayController::refreshLocalAddresses() {
    return win32::resolveLocalAddresses(port_, secretPath_, localAddresses_);
}

void TrayController::run() {
    threadId_.store(GetCurrentThreadId());
    const char className[] = "RinpTrayWindow";

    WNDCLASSA windowClass = {};
    windowClass.lpfnWndProc = &TrayController::windowProc;
    windowClass.hInstance = GetModuleHandleA(nullptr);
    windowClass.lpszClassName = className;

    if (RegisterClassA(&windowClass) == 0) {
        threadId_.store(0);
        return;
    }

    HWND hwnd = CreateWindowExA(
        0,
        className,
        "rinp",
        0,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        windowClass.hInstance,
        this
    );

    if (hwnd == nullptr) {
        UnregisterClassA(className, windowClass.hInstance);
        threadId_.store(0);
        return;
    }

    if (!addTrayIcon(hwnd)) {
        DestroyWindow(hwnd);
        UnregisterClassA(className, windowClass.hInstance);
        threadId_.store(0);
        return;
    }
    trayIconVisible_.store(true);

    MSG message = {};
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_TRAY_SET_VISIBILITY) {
            setTrayIconVisible(hwnd, message.wParam != 0);
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    UnregisterClassA(className, windowClass.hInstance);
    threadId_.store(0);
}

void TrayController::stop() {
    DWORD threadId = threadId_.load();
    if (threadId != 0) PostThreadMessageW(threadId, WM_QUIT, 0, 0);
}

void TrayController::setVisible(bool visible) {
    DWORD threadId = threadId_.load();
    if (threadId != 0) PostThreadMessageW(threadId, WM_TRAY_SET_VISIBILITY, visible, 0);
}

bool TrayController::addTrayIcon(HWND hwnd) {
    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_RINP_APP_ICON));
    if (nid.hIcon == nullptr) nid.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    std::strncpy(nid.szTip, "rinp", sizeof(nid.szTip) - 1);
    return Shell_NotifyIconA(NIM_ADD, &nid) != FALSE;
}

void TrayController::removeTrayIcon(HWND hwnd) {
    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconA(NIM_DELETE, &nid);
}

void TrayController::setTrayIconVisible(HWND hwnd, bool visible) {
    if (visible == trayIconVisible_.load()) return;
    if (visible) {
        if (addTrayIcon(hwnd)) trayIconVisible_.store(true);
    } else {
        removeTrayIcon(hwnd);
        trayIconVisible_.store(false);
    }
}
