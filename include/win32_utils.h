#pragma once

#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

namespace win32 {

void enableDpiAwareness();
std::string formatMessage(const char* format, va_list args);
void showError(const std::string& message);
void reportError(const char* format, ...);
bool deleteCurrentExecutable();
bool launchSelfDeleteAndExit();
bool utf8ToUtf16(const char* utf8, size_t len, std::wstring& out);
bool clearTerminalScrollback(HANDLE console);
void clearScreen();
bool getTextFromClipboard(std::string& text);
bool copyTextToClipboard(HWND owner, const std::string& text);
bool resolveLocalAddresses(
	uint16_t port,
	const std::string& secretPath,
	std::vector<std::string>& addresses
);

} // namespace win32
