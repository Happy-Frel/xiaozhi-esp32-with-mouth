#pragma once

#include "display/lcd_display.h"

#include <string>

/**
 * @brief Otto机器人GIF表情显示类
 * 继承SpiLcdDisplay，通过EmojiCollection添加GIF表情支持
 */
class OttoEmojiDisplay : public SpiLcdDisplay {
   public:
    /**
     * @brief 构造函数，参数与SpiLcdDisplay相同
     */
    OttoEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy);

    virtual ~OttoEmojiDisplay();
    virtual void SetStatus(const char* status) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    virtual void ShowPomodoroCountdown(int remaining_seconds, int total_seconds, const char* label, int duration_ms = 10000) override;
    virtual void SetupUI() override;

   private:
    enum class MouthExpression {
        kNeutral,
        kSmile,
        kLaugh,
        kFunny,
        kLoving,
        kEmbarrassed,
        kConfident,
        kDelicious,
        kWinking,
        kCool,
        kRelaxed,
        kKissy,
        kSad,
        kCrying,
        kSleepy,
        kAngry,
        kSurprised,
        kShocked,
        kThinking,
        kSilly,
        kConfused,
    };

    lv_obj_t* mouth_layer_ = nullptr;
    lv_obj_t* mouth_canvas_ = nullptr;
    lv_obj_t* pomodoro_layer_ = nullptr;
    lv_obj_t* pomodoro_time_box_ = nullptr;
    lv_obj_t* pomodoro_time_shadow_label_ = nullptr;
    lv_obj_t* pomodoro_time_label_ = nullptr;
    lv_obj_t* pomodoro_label_ = nullptr;
    lv_obj_t* pomodoro_bar_ = nullptr;
    lv_draw_buf_t* mouth_canvas_draw_buf_ = nullptr;
    esp_timer_handle_t mouth_timer_ = nullptr;
    esp_timer_handle_t pomodoro_timer_ = nullptr;
    esp_timer_handle_t pomodoro_tick_timer_ = nullptr;
    int64_t pomodoro_target_end_us_ = 0;
    int pomodoro_remaining_seconds_ = 0;
    int pomodoro_total_seconds_ = 0;
    std::string pomodoro_title_;
    MouthExpression mouth_expression_ = MouthExpression::kSmile;
    bool mouth_enabled_ = false;
    bool mouth_speaking_ = false;
    bool mouth_open_ = false;
    int mouth_anim_phase_ = 0;
    bool standby_mouth_smile_ = false;
    bool speaking_status_ = false;
    std::string current_emotion_ = "neutral";

    void InitializeOttoEmojis();
    void SetupPreviewImage();
    void SetupMouth();
    void SetMouthEnabled(bool enabled);
    void SetMouthExpression(MouthExpression expression);
    void SetMouthSpeaking(bool speaking);
    void UpdateMouthShape(bool open);
    void SetMouthLayerHidden(bool hidden);
    void MoveMouthToForeground();
    void RefreshOttoGifEmotion();
    void SetupPomodoroLayer();
    void HidePomodoroLayer();
    void UpdatePomodoroLayer();
};
