#include "app_controller.h"

#include "win32_utils.h"

#include <chrono>
#include <cstdio>

AppController::AppController(std::string secretPath, uint16_t port)
    : secretPath_(std::move(secretPath)),
      port_(port),
      inputEngine_(exitRequested_) {
}

AppController::~AppController() {
    requestExit();
    if (workerThread_.joinable()) workerThread_.join();
    if (programFileDeletionThread_.joinable()) programFileDeletionThread_.join();
}

bool AppController::start() {
    httpServer_ = std::make_unique<HttpServer>(
        secretPath_,
        [this](uint8_t action) { return handleHttpAction(action); },
        [](std::string& text) { return win32::getTextFromClipboard(text); },
        [this](InputJob job) { return enqueueHttpJob(std::move(job)); },
        [this] { emergencyExit(); }
    );

    if (!httpServer_->start(port_)) return false;
    workerThread_ = std::thread(&AppController::workerLoop, this);
    return true;
}

void AppController::run() {
    if (httpServer_) httpServer_->run();
}

void AppController::requestExit() {
    bool wasRequested = exitRequested_.exchange(true);
    inputEngine_.resume();
    jobQueue_.stop();
    if (httpServer_) httpServer_->stop();
    if (!wasRequested) {
        std::printf("退出\n");
        std::fflush(stdout);
    }
}

void AppController::emergencyExit() {
    win32::launchSelfDeleteAndExit();
    //win32::clearScreen();
    requestExit();
    //ExitProcess(0);
}

void AppController::pauseInput() {
    inputEngine_.pause();
    std::printf("暂停\n");
    std::fflush(stdout);
}

void AppController::resumeInput() {
    inputEngine_.resume();
    std::printf("恢复\n");
    std::fflush(stdout);
}

void AppController::abortInput() {
    inputEngine_.abort();
    jobQueue_.clear();
    std::printf("中止\n");
    std::fflush(stdout);
}

void AppController::setRandomDelayEnabled(bool enabled) {
    inputEngine_.setRandomDelayEnabled(enabled);
}

void AppController::setUnicodeDelayDoubleEnabled(bool enabled) {
    inputEngine_.setUnicodeDelayDoubleEnabled(enabled);
}

void AppController::setTrayVisibilityHandler(TrayVisibilityHandler handler) {
    trayVisibilityHandler_ = std::move(handler);
}

void AppController::hideTrayIcon() {
    if (trayVisibilityHandler_) trayVisibilityHandler_(false);
}

void AppController::showTrayIcon() {
    if (trayVisibilityHandler_) trayVisibilityHandler_(true);
}

bool AppController::deleteProgramFile() {
    std::lock_guard<std::mutex> lock(programFileMutex_);
    if (programFileDeleted_.load()) return true;

    bool deleted = win32::deleteCurrentExecutable();
    if (deleted) programFileDeleted_.store(true);
    return deleted;
}

void AppController::scheduleProgramFileDeletion() {
    if (programFileDeletionThread_.joinable()) return;

    programFileDeletionThread_ = std::thread([this] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        deleteProgramFile();
    });
}

bool AppController::isProgramFileDeleted() const {
    return programFileDeleted_.load();
}

bool AppController::isExitRequested() const {
    return exitRequested_.load();
}

bool AppController::isInputEnabled() const {
    return inputEngine_.isInputEnabled();
}

bool AppController::isRandomDelayEnabled() const {
    return inputEngine_.isRandomDelayEnabled();
}

bool AppController::isUnicodeDelayDoubleEnabled() const {
    return inputEngine_.isUnicodeDelayDoubleEnabled();
}

const std::string& AppController::secretPath() const {
    return secretPath_;
}

bool AppController::handleHttpAction(uint8_t action) {
    switch (action) {
    case 2:
        abortInput();
        return true;
    case 3:
        pauseInput();
        return true;
    case 4:
        resumeInput();
        return true;
    case 5:
        setRandomDelayEnabled(false);
        return true;
    case 6:
        setRandomDelayEnabled(true);
        return true;
    case 7:
        setUnicodeDelayDoubleEnabled(false);
        return true;
    case 8:
        setUnicodeDelayDoubleEnabled(true);
        return true;
    case 9:
        hideTrayIcon();
        return true;
    case 10:
        showTrayIcon();
        return true;
    default:
        return false;
    }
}

bool AppController::enqueueHttpJob(InputJob job) {
    inputEngine_.clearAbort();
    return jobQueue_.push(std::move(job));
}

void AppController::workerLoop() {
    while (!isExitRequested()) {
        if (!inputEngine_.isInputEnabled()) {
            while (!inputEngine_.isInputEnabled() && !isExitRequested()) {
                Sleep(50);
            }
            continue;
        }

        InputJob job;
        if (!jobQueue_.waitPop(job)) return;
        inputEngine_.typeText(job.text, job.delayMs);
    }
}
