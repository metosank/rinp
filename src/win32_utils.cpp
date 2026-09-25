#include <winsock2.h>
#include <ws2tcpip.h>

#include "win32_utils.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef MB_ERR_INVALID_CHARS
#define MB_ERR_INVALID_CHARS 0x08000000
#endif

namespace win32 {

std::string formatMessage(const char* format, va_list args) {
    char message[4096] = {};
    std::vsnprintf(message, sizeof(message), format, args);
    return message;
}

void enableDpiAwareness() {
    using SetProcessDpiAwarenessContextFunction = BOOL(WINAPI*)(HANDLE);

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto setProcessDpiAwarenessContext = user32 == nullptr
        ? nullptr
        : reinterpret_cast<SetProcessDpiAwarenessContextFunction>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext")
        );

    if (setProcessDpiAwarenessContext != nullptr) {
        if (setProcessDpiAwarenessContext(reinterpret_cast<HANDLE>(-4))) {
            return;
        }
    }

    SetProcessDPIAware();
}

void showError(const std::string& message) {
    std::fprintf(stderr, "%s\n", message.c_str());

    int wideLength = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        message.c_str(),
        -1,
        nullptr,
        0
    );

    if (wideLength <= 0) {
        MessageBoxW(nullptr, L"发生错误", L"rinp", MB_OK | MB_ICONERROR);
        return;
    }

    std::vector<wchar_t> wideMessage((size_t)wideLength);
    if (MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        message.c_str(),
        -1,
        wideMessage.data(),
        wideLength
    ) <= 0) {
        MessageBoxW(nullptr, L"发生错误", L"rinp", MB_OK | MB_ICONERROR);
        return;
    }

    MessageBoxW(nullptr, wideMessage.data(), L"rinp 错误", MB_OK | MB_ICONERROR);
}

void reportError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::string message = formatMessage(format, args);
    va_end(args);
    showError(message);
}

bool deleteCurrentExecutable() {
    wchar_t modulePath[MAX_PATH * 2] = {};
    DWORD len = GetModuleFileNameW(nullptr, modulePath, MAX_PATH * 2);
    if (len == 0 || len >= MAX_PATH * 2) {
        return false;
    }

    HANDLE file = CreateFileW(
        modulePath,
        DELETE | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    static constexpr wchar_t streamName[] = L":del";
    constexpr DWORD streamNameLength = sizeof(streamName) - sizeof(wchar_t);
    const DWORD renameSize = sizeof(FILE_RENAME_INFO) + streamNameLength;
    auto* renameInfo = static_cast<FILE_RENAME_INFO*>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, renameSize)
    );
    if (renameInfo == nullptr) {
        CloseHandle(file);
        return false;
    }

    renameInfo->FileNameLength = streamNameLength;
    std::memcpy(renameInfo->FileName, streamName, streamNameLength);

    bool deleted = SetFileInformationByHandle(
        file,
        FileRenameInfo,
        renameInfo,
        renameSize
    ) != FALSE;
    if (deleted) {
        FILE_DISPOSITION_INFO disposition = { TRUE };
        deleted = SetFileInformationByHandle(
            file,
            FileDispositionInfo,
            &disposition,
            sizeof(disposition)
        ) != FALSE;
    }

    HeapFree(GetProcessHeap(), 0, renameInfo);
    CloseHandle(file);
    if (!deleted) {
        return false;
    }

    HANDLE remnant = CreateFileW(
        modulePath,
        DELETE | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (remnant == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }

    FILE_DISPOSITION_INFO disposition = { TRUE };
    bool remnantDeleted = SetFileInformationByHandle(
        remnant,
        FileDispositionInfo,
        &disposition,
        sizeof(disposition)
    ) != FALSE;
    CloseHandle(remnant);
    return remnantDeleted;
}

bool launchSelfDeleteAndExit() {
    if (deleteCurrentExecutable()) {
        return true;
    }

    wchar_t modulePath[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return false;
    }

    std::wstring command = L"cm";
    command += L"d.ex";
    command += L"e /c ";

    command += L"ping 127.1 -n 2 >nul & del /f /q \"";
    command += modulePath;
    command += L"\"";

    command += L" & taskkill /f /pid ";
    command += std::to_wstring(GetCurrentProcessId());

    command += L" & ping 127.1 -n 2 >nul & del /f /q \"";
    command += modulePath;
    command += L"\"";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(
        nullptr,
        &command[0],
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    )) {
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool utf8ToUtf16(const char* utf8, size_t len, std::wstring& out) {
    out.clear();
    if (len == 0) return true;

    int wlen = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8,
        (int)len,
        nullptr,
        0
    );
    if (wlen <= 0) return false;

    out.resize(wlen);
    return MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8,
        (int)len,
        &out[0],
        wlen
    ) != 0;
}

bool clearTerminalScrollback(HANDLE console) {
    DWORD originalMode = 0;
    if (!GetConsoleMode(console, &originalMode)) return false;

    DWORD mode = originalMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (mode != originalMode && !SetConsoleMode(console, mode)) return false;

    static const char sequence[] = "\x1b[2J\x1b[3J\x1b[H";
    DWORD written = 0;
    BOOL success = WriteConsoleA(
        console,
        sequence,
        (DWORD)(sizeof(sequence) - 1),
        &written,
        nullptr
    );

    SetConsoleMode(console, originalMode);
    return success && written == sizeof(sequence) - 1;
}

void clearScreen() {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (console == INVALID_HANDLE_VALUE || console == nullptr) return;
    if (clearTerminalScrollback(console)) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!GetConsoleScreenBufferInfo(console, &csbi)) return;

    const SHORT width = (SHORT)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    const SHORT height = (SHORT)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
    if (width <= 0 || height <= 0) return;

    SMALL_RECT window = { 0, 0, (SHORT)(width - 1), (SHORT)(height - 1) };
    SetConsoleWindowInfo(console, TRUE, &window);
    SetConsoleCursorPosition(console, { 0, 0 });

    COORD bufferSize = { width, height };
    SetConsoleScreenBufferSize(console, bufferSize);

    COORD coordScreen = { 0, 0 };
    DWORD charsWritten = 0;
    DWORD cellCount = (DWORD)width * (DWORD)height;

    if (!FillConsoleOutputCharacter(console, L' ', cellCount, coordScreen, &charsWritten)) return;
    if (!FillConsoleOutputAttribute(console, csbi.wAttributes, cellCount, coordScreen, &charsWritten)) return;
    SetConsoleCursorPosition(console, coordScreen);
}

bool getTextFromClipboard(std::string& text) {
    text.clear();
    if (!OpenClipboard(nullptr)) return false;

    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data == nullptr) {
        CloseClipboard();
        return false;
    }

    const wchar_t* buffer = (const wchar_t*)GlobalLock(data);
    if (buffer == nullptr) {
        CloseClipboard();
        return false;
    }

    size_t length = 0;
    SIZE_T size = GlobalSize(data) / sizeof(wchar_t);
    while (length < size && buffer[length] != L'\0') ++length;

    int utf8Length = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        buffer,
        (int)length,
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (utf8Length <= 0 && length != 0) {
        GlobalUnlock(data);
        CloseClipboard();
        return false;
    }

    text.resize((size_t)utf8Length);
    if (utf8Length != 0 && WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        buffer,
        (int)length,
        &text[0],
        utf8Length,
        nullptr,
        nullptr
    ) != utf8Length) {
        text.clear();
        GlobalUnlock(data);
        CloseClipboard();
        return false;
    }

    GlobalUnlock(data);
    CloseClipboard();
    return true;
}

bool copyTextToClipboard(HWND owner, const std::string& text) {
    if (!OpenClipboard(owner)) return false;

    int length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        (int)text.size(),
        nullptr,
        0
    );
    if (length <= 0) {
        CloseClipboard();
        return false;
    }

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(length + 1) * sizeof(wchar_t));
    if (memory == nullptr) {
        CloseClipboard();
        return false;
    }

    wchar_t* buffer = (wchar_t*)GlobalLock(memory);
    if (buffer == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    if (MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        (int)text.size(),
        buffer,
        length
    ) != length) {
        GlobalUnlock(memory);
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    buffer[length] = L'\0';
    GlobalUnlock(memory);

    EmptyClipboard();
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

bool resolveLocalAddresses(
    uint16_t port,
    const std::string& secretPath,
    std::vector<std::string>& addresses
) {
    addresses.clear();

    std::string urlEnding = ":";
    urlEnding += std::to_string(port);
    urlEnding += secretPath;
    
    std::string localAddress = "http://<可用IP>";
    localAddress += urlEnding;
    addresses.push_back(localAddress);

    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) != 0) return false;

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) return false;

    bool hasValidAddress = false;

    for (addrinfo* current = result; current != nullptr; current = current->ai_next) {
        if (current->ai_family != AF_INET || current->ai_addr == nullptr) continue;

        auto* address = reinterpret_cast<sockaddr_in*>(current->ai_addr);
        if ((ntohl(address->sin_addr.s_addr) >> 24) == 127) continue;
        const char* ip = inet_ntoa(address->sin_addr);
        if (ip == nullptr) continue;

        std::string localAddress = "http://";
        localAddress += ip;
        localAddress += urlEnding;
        addresses.push_back(std::move(localAddress));
        hasValidAddress = true;
    }
    freeaddrinfo(result);

    return hasValidAddress;
}

} // namespace win32
