#include "input_engine.h"

#include <windows.h>

#include <algorithm>
#include <random>

#ifndef KEYEVENTF_UNICODE
#define KEYEVENTF_UNICODE 0x0004
#endif

#ifndef KEYEVENTF_KEYUP
#define KEYEVENTF_KEYUP 0x0002
#endif

namespace {

INPUT makeKeyboardInput(WORD vk, bool up) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    return input;
}

} // namespace

InputEngine::InputEngine(std::atomic<bool>& exitRequested)
    : exitRequested_(exitRequested) {
}

void InputEngine::waitForInputGate() {
    std::unique_lock<std::mutex> lock(pauseMutex_);
    pauseCv_.wait(lock, [this] {
        return inputEnabled_.load() || abortRequested_.load() || exitRequested_.load();
    });
}

bool InputEngine::waitForInputDelay(uint32_t delayMs) {
    if (exitRequested_.load() || abortRequested_.load()) return false;

    if (delayMs == 0) {
        waitForInputGate();
        return !exitRequested_.load() && !abortRequested_.load();
    }

    uint32_t remaining = delayMs;
    while (remaining > 0) {
        if (exitRequested_.load() || abortRequested_.load()) return false;

        waitForInputGate();
        if (exitRequested_.load() || abortRequested_.load()) return false;

        uint32_t chunk = std::min<uint32_t>(remaining, 50);
        Sleep(chunk);
        remaining -= chunk;
    }

    return !exitRequested_.load() && !abortRequested_.load();
}

uint32_t InputEngine::getCharacterDelay(uint32_t delayMs, bool unicodeInput) {
    uint64_t effectiveDelay = delayMs;
    if (unicodeInput && unicodeDelayDoubleEnabled_.load()) {
        effectiveDelay *= 2;
    }

    if (!randomDelayEnabled_.load() || effectiveDelay == 0) {
        return (uint32_t)effectiveDelay;
    }

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> percentage(200, 1800);
    return (uint32_t)((effectiveDelay * (uint32_t)percentage(rng)) / 1000);
}

bool InputEngine::areUserModifiersPressed() const {
    static constexpr int modifierKeys[] = {
        VK_LWIN,
        VK_RWIN,
        VK_LMENU,
        VK_RMENU,
        VK_LCONTROL,
        VK_RCONTROL,
        VK_LSHIFT,
        VK_RSHIFT
    };

    for (int key : modifierKeys) {
        if ((GetAsyncKeyState(key) & 0x8000) != 0) return true;
    }

    return false;
}

bool InputEngine::waitForUserModifiersReleased() const {
    while (areUserModifiersPressed()) {
        if (exitRequested_.load() || abortRequested_.load()) return false;
        Sleep(100);
    }

    return !exitRequested_.load() && !abortRequested_.load();
}

void InputEngine::sendVirtualKey(unsigned short vk) const {
    if (!waitForUserModifiersReleased()) return;

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[0].ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void InputEngine::sendUnicodeUnit(wchar_t wc) const {
    if (!waitForUserModifiersReleased()) return;

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = wc;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void InputEngine::sendVirtualKeyWithShift(unsigned short vk, bool needShift) const {
    if (!waitForUserModifiersReleased()) return;

    INPUT inputs[6] = {};
    int count = 0;
    if (needShift) inputs[count++] = makeKeyboardInput(VK_SHIFT, false);
    inputs[count++] = makeKeyboardInput(vk, false);
    inputs[count++] = makeKeyboardInput(vk, true);
    if (needShift) inputs[count++] = makeKeyboardInput(VK_SHIFT, true);
    SendInput(count, inputs, sizeof(INPUT));
}

bool InputEngine::shouldUseKeyboardForChar(wchar_t ch) {
    return ch >= 0x20 && ch <= 0x7E;
}

bool InputEngine::trySendCharViaKeyboard(wchar_t ch) const {
    if (!shouldUseKeyboardForChar(ch)) return false;

    SHORT scan = -1;
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
        HKL keyboardLayout = GetKeyboardLayout(threadId);
        if (keyboardLayout) scan = VkKeyScanExW(ch, keyboardLayout);
    }

    if (scan == -1) scan = VkKeyScanW(ch);
    if (scan == -1) return false;

    WORD scanWord = (WORD)scan;
    BYTE vk = LOBYTE(scanWord);
    BYTE modifiers = HIBYTE(scanWord);
    if (vk == 0 || vk == 0xFF) return false;
    if ((modifiers & 0xFE) != 0) return false;

    bool needShift = (modifiers & 0x01) != 0;
    bool asciiLetter =
        (ch >= L'A' && ch <= L'Z') ||
        (ch >= L'a' && ch <= L'z');

    if (asciiLetter && (GetKeyState(VK_CAPITAL) & 0x0001) != 0) {
        needShift = !needShift;
    }

    sendVirtualKeyWithShift((WORD)vk, needShift);
    return true;
}

void InputEngine::typeText(const std::wstring& text, uint32_t delayMs) {
    std::lock_guard<std::mutex> lock(inputMutex_);

    for (size_t index = 0; index < text.size();) {
        if (!waitForInputDelay(0)) return;

        wchar_t ch = text[index];
        bool unicodeInput = false;

        if (ch == L'\r') {
            if (index + 1 < text.size() && text[index + 1] == L'\n') {
                index += 2;
            } else {
                ++index;
            }
            sendVirtualKey(VK_RETURN);
        } else if (ch == L'\n') {
            ++index;
            sendVirtualKey(VK_RETURN);
        } else if (ch == L'\t') {
            ++index;
            sendVirtualKey(VK_TAB);
        } else if (
            ch >= 0xD800 && ch <= 0xDBFF &&
            index + 1 < text.size() &&
            text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF
        ) {
            sendUnicodeUnit(text[index]);
            sendUnicodeUnit(text[index + 1]);
            unicodeInput = true;
            index += 2;
        } else {
            if (!trySendCharViaKeyboard(ch)) {
                sendUnicodeUnit(ch);
                unicodeInput = true;
            }
            ++index;
        }

        if (!waitForInputDelay(getCharacterDelay(delayMs, unicodeInput))) return;
    }
}

void InputEngine::pause() {
    inputEnabled_.store(false);
    pauseCv_.notify_all();
}

void InputEngine::resume() {
    inputEnabled_.store(true);
    pauseCv_.notify_all();
}

void InputEngine::abort() {
    abortRequested_.store(true);
    pauseCv_.notify_all();
}

void InputEngine::clearAbort() {
    abortRequested_.store(false);
}

void InputEngine::setRandomDelayEnabled(bool enabled) {
    randomDelayEnabled_.store(enabled);
}

void InputEngine::setUnicodeDelayDoubleEnabled(bool enabled) {
    unicodeDelayDoubleEnabled_.store(enabled);
}

bool InputEngine::isInputEnabled() const {
    return inputEnabled_.load();
}

bool InputEngine::isRandomDelayEnabled() const {
    return randomDelayEnabled_.load();
}

bool InputEngine::isUnicodeDelayDoubleEnabled() const {
    return unicodeDelayDoubleEnabled_.load();
}
