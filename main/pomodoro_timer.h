#pragma once

#include <esp_timer.h>
#include <mutex>
#include <string>

class PomodoroTimer {
public:
    static constexpr int kCountdownDisplayDurationMs = 10000;

    static PomodoroTimer& GetInstance();

    void Start(int duration_minutes, const std::string& label);
    void Cancel();
    std::string GetStatusJson() const;
    bool ShowCountdown(int duration_ms = kCountdownDisplayDurationMs);

private:
    PomodoroTimer() = default;

    bool IsRunningLocked(int64_t now_us) const;
    int RemainingSecondsLocked(int64_t now_us) const;
    void ShowCountdownLocked(int duration_ms);
    void OnFinished();

    mutable std::mutex mutex_;
    bool running_ = false;
    int64_t start_time_us_ = 0;
    int total_seconds_ = 0;
    std::string label_;
    esp_timer_handle_t finish_timer_ = nullptr;
};
