#pragma once

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <string>

class InputEngine {
public:
    explicit InputEngine(std::atomic<bool>& exitRequested);

    void typeText(const std::wstring& text, uint32_t delayMs);
    void pause();
    void resume();
    void abort();
    void clearAbort();

    void setRandomDelayEnabled(bool enabled);
    void setUnicodeDelayDoubleEnabled(bool enabled);

    bool isInputEnabled() const;
    bool isRandomDelayEnabled() const;
    bool isUnicodeDelayDoubleEnabled() const;

private:
    void waitForInputGate();
    bool waitForInputDelay(uint32_t delayMs);
    uint32_t getCharacterDelay(uint32_t delayMs, bool unicodeInput);
    bool areUserModifiersPressed() const;
    bool waitForUserModifiersReleased() const;
    void sendVirtualKey(unsigned short vk) const;
    void sendUnicodeUnit(wchar_t wc) const;
    bool trySendCharViaKeyboard(wchar_t ch) const;
    static bool shouldUseKeyboardForChar(wchar_t ch);
    void sendVirtualKeyWithShift(unsigned short vk, bool needShift) const;

    std::atomic<bool>& exitRequested_;
    mutable std::mutex inputMutex_;
    mutable std::mutex pauseMutex_;
    std::condition_variable pauseCv_;
    std::atomic<bool> abortRequested_{false};
    std::atomic<bool> inputEnabled_{true};
    std::atomic<bool> randomDelayEnabled_{true};
    std::atomic<bool> unicodeDelayDoubleEnabled_{true};
};
