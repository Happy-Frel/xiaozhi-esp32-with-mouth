#include "pomodoro_timer.h"

#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "display.h"

#include <algorithm>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>

#define TAG "PomodoroTimer"

namespace {
constexpr int64_t kUsPerSecond = 1000000;
constexpr int kMaxDurationMinutes = 24 * 60;

std::string EscapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}
}

PomodoroTimer& PomodoroTimer::GetInstance() {
    static PomodoroTimer instance;
    return instance;
}

void PomodoroTimer::Start(int duration_minutes, const std::string& label) {
    duration_minutes = std::clamp(duration_minutes, 1, kMaxDurationMinutes);

    std::lock_guard<std::mutex> lock(mutex_);
    if (finish_timer_ == nullptr) {
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                PomodoroTimer* timer = static_cast<PomodoroTimer*>(arg);
                timer->OnFinished();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "pomodoro_finish_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &finish_timer_));
    } else {
        esp_timer_stop(finish_timer_);
    }

    running_ = true;
    start_time_us_ = esp_timer_get_time();
    total_seconds_ = duration_minutes * 60;
    label_ = label.empty() ? "Pomodoro" : label;

    ESP_LOGI(TAG, "Started: label=%s duration=%d minutes", label_.c_str(), duration_minutes);
    ESP_ERROR_CHECK(esp_timer_start_once(finish_timer_, static_cast<uint64_t>(total_seconds_) * kUsPerSecond));
    ShowCountdownLocked(kCountdownDisplayDurationMs);
}

void PomodoroTimer::Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (finish_timer_ != nullptr) {
        esp_timer_stop(finish_timer_);
    }
    running_ = false;
    start_time_us_ = 0;
    total_seconds_ = 0;
    label_.clear();
    ESP_LOGI(TAG, "Cancelled");
}

std::string PomodoroTimer::GetStatusJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int64_t now_us = esp_timer_get_time();
    bool running = IsRunningLocked(now_us);
    int remaining_seconds = running ? RemainingSecondsLocked(now_us) : 0;

    return std::string("{\"running\":") + (running ? "true" : "false") +
           ",\"remaining_seconds\":" + std::to_string(remaining_seconds) +
           ",\"total_seconds\":" + std::to_string(running ? total_seconds_ : 0) +
           ",\"label\":\"" + EscapeJsonString(running ? label_ : "") + "\"}";
}

bool PomodoroTimer::ShowCountdown(int duration_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    int64_t now_us = esp_timer_get_time();
    if (!IsRunningLocked(now_us)) {
        running_ = false;
        return false;
    }

    ShowCountdownLocked(duration_ms);
    return true;
}

bool PomodoroTimer::IsRunningLocked(int64_t now_us) const {
    return running_ && total_seconds_ > 0 && RemainingSecondsLocked(now_us) > 0;
}

int PomodoroTimer::RemainingSecondsLocked(int64_t now_us) const {
    if (!running_ || total_seconds_ <= 0) {
        return 0;
    }

    int elapsed_seconds = static_cast<int>((now_us - start_time_us_) / kUsPerSecond);
    return std::max(0, total_seconds_ - elapsed_seconds);
}

void PomodoroTimer::ShowCountdownLocked(int duration_ms) {
    int remaining_seconds = RemainingSecondsLocked(esp_timer_get_time());
    int total_seconds = total_seconds_;
    std::string label = label_;

    Application::GetInstance().Schedule([remaining_seconds, total_seconds, label, duration_ms]() {
        auto display = Board::GetInstance().GetDisplay();
        display->ShowPomodoroCountdown(remaining_seconds, total_seconds, label.c_str(), duration_ms);
    });
}

void PomodoroTimer::OnFinished() {
    std::string label;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
        label = label_;
        start_time_us_ = 0;
        total_seconds_ = 0;
        label_.clear();
    }

    ESP_LOGI(TAG, "Finished: label=%s", label.c_str());
    Application::GetInstance().Schedule([label]() {
        auto display = Board::GetInstance().GetDisplay();
        display->ShowPomodoroCountdown(0, 1, label.empty() ? "Pomodoro done" : label.c_str(), kCountdownDisplayDurationMs);
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_EXCLAMATION);
    });
}
