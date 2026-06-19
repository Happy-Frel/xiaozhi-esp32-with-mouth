#include "otto_emoji_display.h"

#include <esp_err.h>
#include <esp_log.h>

#include <cstring>

#include "assets.h"
#include "assets/lang_config.h"
#include "display/lvgl_display/emoji_collection.h"
#include "display/lvgl_display/lvgl_image.h"
#include "display/lvgl_display/lvgl_theme.h"

#define TAG "OttoEmojiDisplay"

namespace {
constexpr int kMouthSamplesPerAxis = 3;

void SetObjectHidden(lv_obj_t* obj, bool hidden) {
    if (obj == nullptr) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

float MouthCurve(float x) {
    const float d = x * 2.0f - 1.0f;
    const float v = 1.0f - d * d;
    return v > 0.0f ? v : 0.0f;
}

float AbsF(float value) {
    return value < 0.0f ? -value : value;
}

float Mix(float a, float b, float t) {
    return a + (b - a) * t;
}

float OvalValue(float x, float y, float cx, float cy, float rx, float ry) {
    const float dx = (x - cx) / rx;
    const float dy = (y - cy) / ry;
    return dx * dx + dy * dy;
}

lv_opa_t CoverageToOpa(int coverage) {
    const int sample_count = kMouthSamplesPerAxis * kMouthSamplesPerAxis;
    if (coverage <= 0) {
        return LV_OPA_TRANSP;
    }
    if (coverage >= sample_count) {
        return LV_OPA_COVER;
    }
    return static_cast<lv_opa_t>((coverage * 255 + sample_count / 2) / sample_count);
}

void DrawSoftCircle(lv_obj_t* canvas, int canvas_width, int canvas_height, float cx, float cy,
                    float radius, lv_color_t color) {
    const float r2 = radius * radius;
    for (int y = 0; y < canvas_height; ++y) {
        for (int x = 0; x < canvas_width; ++x) {
            int coverage = 0;
            for (int sy = 0; sy < kMouthSamplesPerAxis; ++sy) {
                for (int sx = 0; sx < kMouthSamplesPerAxis; ++sx) {
                    const float px = x + (sx + 0.5f) / kMouthSamplesPerAxis;
                    const float py = y + (sy + 0.5f) / kMouthSamplesPerAxis;
                    const float dx = px - cx;
                    const float dy = py - cy;
                    if (dx * dx + dy * dy <= r2) {
                        ++coverage;
                    }
                }
            }
            if (coverage > 0) {
                lv_canvas_set_px(canvas, x, y, color, CoverageToOpa(coverage));
            }
        }
    }
}

void DrawSoftOval(lv_obj_t* canvas, int canvas_width, int canvas_height, float cx, float cy,
                  float rx, float ry, lv_color_t color) {
    for (int y = 0; y < canvas_height; ++y) {
        for (int x = 0; x < canvas_width; ++x) {
            int coverage = 0;
            for (int sy = 0; sy < kMouthSamplesPerAxis; ++sy) {
                for (int sx = 0; sx < kMouthSamplesPerAxis; ++sx) {
                    const float px = x + (sx + 0.5f) / kMouthSamplesPerAxis;
                    const float py = y + (sy + 0.5f) / kMouthSamplesPerAxis;
                    if (OvalValue(px, py, cx, cy, rx, ry) <= 1.0f) {
                        ++coverage;
                    }
                }
            }
            if (coverage > 0) {
                lv_canvas_set_px(canvas, x, y, color, CoverageToOpa(coverage));
            }
        }
    }
}

void DrawCurvedStroke(lv_obj_t* canvas, int canvas_width, int canvas_height, float left,
                      float right, float left_y, float mid_y, float right_y, float thickness,
                      lv_color_t color) {
    const float width = right - left;
    if (width <= 0.0f) {
        return;
    }
    for (int y = 0; y < canvas_height; ++y) {
        for (int x = 0; x < canvas_width; ++x) {
            int coverage = 0;
            for (int sy = 0; sy < kMouthSamplesPerAxis; ++sy) {
                for (int sx = 0; sx < kMouthSamplesPerAxis; ++sx) {
                    const float px = x + (sx + 0.5f) / kMouthSamplesPerAxis;
                    const float py = y + (sy + 0.5f) / kMouthSamplesPerAxis;
                    if (px < left || px > right) {
                        continue;
                    }
                    const float nx = (px - left) / width;
                    const float base_y = Mix(left_y, right_y, nx);
                    const float curve_y = base_y + (mid_y - base_y) * MouthCurve(nx);
                    if (AbsF(py - curve_y) <= thickness * 0.5f) {
                        ++coverage;
                    }
                }
            }
            if (coverage > 0) {
                lv_canvas_set_px(canvas, x, y, color, CoverageToOpa(coverage));
            }
        }
    }
}

}  // namespace

OttoEmojiDisplay::OttoEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   int width, int height, int offset_x, int offset_y, bool mirror_x,
                                   bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {}

OttoEmojiDisplay::~OttoEmojiDisplay() {
    if (mouth_timer_ != nullptr) {
        esp_timer_stop(mouth_timer_);
        esp_timer_delete(mouth_timer_);
        mouth_timer_ = nullptr;
    }

    if (mouth_layer_ != nullptr) {
        DisplayLockGuard lock(this);
        lv_obj_del(mouth_layer_);
        mouth_layer_ = nullptr;
        mouth_canvas_ = nullptr;
        if (mouth_canvas_draw_buf_ != nullptr) {
            lv_draw_buf_destroy(mouth_canvas_draw_buf_);
            mouth_canvas_draw_buf_ = nullptr;
        }
    }
}

void OttoEmojiDisplay::SetupUI() {
    // Prevent duplicate calls - parent SetupUI() will also check, but check here for early return
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }

    // Call parent SetupUI() first to create all lvgl objects
    SpiLcdDisplay::SetupUI();

    // UI 对象创建完成后切换主题
    auto* dark_theme = LvglThemeManager::GetInstance().GetTheme("dark");
    if (dark_theme != nullptr) {
        SetTheme(dark_theme);
    }

    // Setup preview image after UI is initialized - release lock before calling SetEmotion
    // to avoid deadlock (SetEmotion also acquires DisplayLockGuard internally)
    {
        DisplayLockGuard lock(this);
        lv_obj_set_size(preview_image_, width_, height_);
    }

    // Mouths are baked into the Otto GIF assets, so no runtime canvas overlay is needed.
    SpiLcdDisplay::SetEmotion("staticstate");
}

void OttoEmojiDisplay::SetupMouth() {
    DisplayLockGuard lock(this);
    if (mouth_layer_ != nullptr) {
        return;
    }

    mouth_layer_ = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(mouth_layer_);
    lv_obj_set_size(mouth_layer_, width_, height_);
    lv_obj_clear_flag(mouth_layer_, LV_OBJ_FLAG_SCROLLABLE);

    const int canvas_width = width_ / 2;
    const int canvas_height = height_ / 4;
    mouth_canvas_draw_buf_ = lv_draw_buf_create(canvas_width, canvas_height,
                                                LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    if (mouth_canvas_draw_buf_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create mouth canvas draw buffer");
        return;
    }

    mouth_canvas_ = lv_canvas_create(mouth_layer_);
    lv_canvas_set_draw_buf(mouth_canvas_, mouth_canvas_draw_buf_);
    lv_obj_set_size(mouth_canvas_, canvas_width, canvas_height);
    lv_obj_clear_flag(mouth_canvas_, LV_OBJ_FLAG_SCROLLABLE);
    lv_canvas_fill_bg(mouth_canvas_, lv_color_black(), LV_OPA_TRANSP);

    UpdateMouthShape(false);
    MoveMouthToForeground();

    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            auto* display = static_cast<OttoEmojiDisplay*>(arg);
            DisplayLockGuard lock(display);
            if (!display->mouth_speaking_ || display->mouth_layer_ == nullptr) {
                return;
            }
            display->mouth_open_ = !display->mouth_open_;
            display->mouth_anim_phase_ = (display->mouth_anim_phase_ + 1) % 4;
            display->UpdateMouthShape(display->mouth_open_);
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "otto_mouth",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &mouth_timer_));
}

void OttoEmojiDisplay::SetMouthExpression(MouthExpression expression) {
    mouth_expression_ = expression;
    if (!mouth_speaking_) {
        mouth_open_ = false;
        UpdateMouthShape(false);
    }
}

void OttoEmojiDisplay::SetMouthEnabled(bool enabled) {
    mouth_enabled_ = enabled;
    if (!enabled) {
        mouth_speaking_ = false;
        mouth_open_ = false;
        mouth_anim_phase_ = 0;
        if (mouth_timer_ != nullptr) {
            esp_timer_stop(mouth_timer_);
        }
    }
    UpdateMouthShape(mouth_open_);
}

void OttoEmojiDisplay::SetMouthSpeaking(bool speaking) {
    if (mouth_layer_ == nullptr || mouth_timer_ == nullptr) {
        return;
    }

    if (!mouth_enabled_ && speaking) {
        return;
    }

    if (mouth_speaking_ == speaking) {
        return;
    }

    mouth_speaking_ = speaking;
    mouth_open_ = speaking;
    mouth_anim_phase_ = 0;
    UpdateMouthShape(mouth_open_);

    esp_timer_stop(mouth_timer_);
    if (speaking) {
        ESP_ERROR_CHECK(esp_timer_start_periodic(mouth_timer_, 140 * 1000));
    }
}

void OttoEmojiDisplay::UpdateMouthShape(bool open) {
    if (mouth_layer_ == nullptr || mouth_canvas_ == nullptr) {
        return;
    }

    if (!mouth_enabled_) {
        SetMouthLayerHidden(true);
        return;
    }

    SetMouthLayerHidden(false);

    const int canvas_width = lv_obj_get_width(mouth_canvas_);
    const int canvas_height = lv_obj_get_height(mouth_canvas_);
    const int mouth_y = height_ / 4 + height_ / 16;

    lv_color_t accent = lv_color_hex(0xff7050);
    lv_color_t inner = lv_color_hex(0x211014);
    lv_color_t tongue = lv_color_hex(0xff6f91);
    lv_color_t highlight = lv_color_hex(0xfff1dc);
    if (mouth_expression_ == MouthExpression::kSad ||
        mouth_expression_ == MouthExpression::kCrying ||
        mouth_expression_ == MouthExpression::kSleepy) {
        accent = lv_color_hex(0x70c8ff);
        inner = lv_color_hex(0x102535);
    } else if (mouth_expression_ == MouthExpression::kAngry) {
        accent = lv_color_hex(0xff4a4a);
        inner = lv_color_hex(0x2a0f0f);
    } else if (mouth_expression_ == MouthExpression::kSurprised ||
               mouth_expression_ == MouthExpression::kShocked) {
        accent = lv_color_hex(0xffc15a);
        inner = lv_color_hex(0x28170b);
    } else if (mouth_expression_ == MouthExpression::kThinking ||
               mouth_expression_ == MouthExpression::kConfused) {
        accent = lv_color_hex(0xe8d16b);
        inner = lv_color_hex(0x2a2410);
    } else if (mouth_expression_ == MouthExpression::kKissy ||
               mouth_expression_ == MouthExpression::kLoving) {
        accent = lv_color_hex(0xff7bb0);
        inner = lv_color_hex(0x321322);
    } else if (mouth_expression_ == MouthExpression::kCool) {
        accent = lv_color_hex(0xb7e6ff);
        inner = lv_color_hex(0x14202a);
    }

    lv_display_t* display = lv_obj_get_display(mouth_canvas_);
    lv_display_enable_invalidation(display, false);
    lv_canvas_fill_bg(mouth_canvas_, lv_color_black(), LV_OPA_TRANSP);

    const bool force_open = mouth_expression_ == MouthExpression::kLaugh ||
                            mouth_expression_ == MouthExpression::kFunny ||
                            mouth_expression_ == MouthExpression::kDelicious ||
                            mouth_expression_ == MouthExpression::kSurprised ||
                            mouth_expression_ == MouthExpression::kShocked ||
                            mouth_expression_ == MouthExpression::kSilly;
    if (open || force_open) {
        float mouth_width = canvas_width * 0.70f;
        float open_height = canvas_height * (open ? 0.44f : 0.30f);
        float mouth_center_x = canvas_width * 0.5f;
        float mouth_center_y = canvas_height * 0.46f;
        float tilt = 0.0f;
        float edge_gap_scale = 0.22f;
        bool show_tongue = mouth_expression_ != MouthExpression::kSurprised &&
                            mouth_expression_ != MouthExpression::kShocked;
        bool show_highlight = mouth_expression_ == MouthExpression::kSurprised ||
                              mouth_expression_ == MouthExpression::kShocked;
        bool show_teeth = false;

        if (mouth_speaking_) {
            if (mouth_anim_phase_ == 0) {
                mouth_width = canvas_width * 0.46f;
                open_height = canvas_height * 0.30f;
                mouth_center_x -= canvas_width * 0.03f;
            } else if (mouth_anim_phase_ == 1) {
                mouth_width = canvas_width * 0.72f;
                open_height = canvas_height * 0.46f;
            } else if (mouth_anim_phase_ == 2) {
                mouth_width = canvas_width * 0.58f;
                open_height = canvas_height * 0.52f;
                mouth_center_x += canvas_width * 0.04f;
                tilt = canvas_height * 0.06f;
            } else {
                mouth_width = canvas_width * 0.36f;
                open_height = canvas_height * 0.28f;
                mouth_center_x -= canvas_width * 0.02f;
            }
        }
        if (mouth_expression_ == MouthExpression::kLaugh) {
            mouth_width = canvas_width * 0.78f;
            open_height = canvas_height * 0.50f;
            mouth_center_y = canvas_height * 0.48f;
            edge_gap_scale = 0.10f;
            show_teeth = true;
        } else if (mouth_expression_ == MouthExpression::kFunny) {
            mouth_width = canvas_width * 0.66f;
            open_height = canvas_height * 0.46f;
            mouth_center_x -= canvas_width * 0.04f;
            mouth_center_y = canvas_height * 0.49f;
            tilt = canvas_height * 0.10f;
            edge_gap_scale = 0.18f;
        } else if (mouth_expression_ == MouthExpression::kDelicious ||
                   mouth_expression_ == MouthExpression::kSilly) {
            mouth_width = canvas_width * 0.56f;
            open_height = canvas_height * 0.42f;
            mouth_center_x += mouth_expression_ == MouthExpression::kSilly ? canvas_width * 0.04f
                                                                            : 0.0f;
            mouth_center_y = canvas_height * 0.46f;
            tilt = mouth_expression_ == MouthExpression::kSilly ? -canvas_height * 0.12f : 0.0f;
            edge_gap_scale = 0.26f;
        } else if (mouth_expression_ == MouthExpression::kSurprised) {
            mouth_width = canvas_width * 0.46f;
            open_height = canvas_height * 0.58f;
            edge_gap_scale = 0.70f;
            show_tongue = false;
            show_highlight = true;
        } else if (mouth_expression_ == MouthExpression::kShocked) {
            mouth_width = canvas_width * 0.58f;
            open_height = canvas_height * 0.60f;
            mouth_center_y = canvas_height * 0.47f;
            edge_gap_scale = 0.34f;
            show_tongue = false;
            show_teeth = true;
            show_highlight = true;
        } else if (mouth_expression_ == MouthExpression::kAngry) {
            open_height = canvas_height * 0.34f;
            tilt = -canvas_height * 0.08f;
            show_tongue = false;
        } else if (mouth_expression_ == MouthExpression::kThinking ||
                   mouth_expression_ == MouthExpression::kConfused ||
                   mouth_expression_ == MouthExpression::kConfident) {
            mouth_width = canvas_width * 0.54f;
            open_height = canvas_height * 0.32f;
            mouth_center_x += canvas_width * 0.05f;
            tilt = canvas_height * 0.10f;
            show_tongue = false;
        }

        const float left = mouth_center_x - mouth_width * 0.5f;
        const float right = left + mouth_width;
        const float center_y = mouth_center_y;
        const float edge_gap = open_height * edge_gap_scale;
        const float border = (mouth_expression_ == MouthExpression::kSurprised ||
                              mouth_expression_ == MouthExpression::kShocked)
                                 ? 3.0f
                                 : 3.6f;

        for (int y = 0; y < canvas_height; ++y) {
            for (int x = 0; x < canvas_width; ++x) {
                int fill_count = 0;
                int border_count = 0;
                int tongue_count = 0;
                int highlight_count = 0;
                for (int sy = 0; sy < kMouthSamplesPerAxis; ++sy) {
                    for (int sx = 0; sx < kMouthSamplesPerAxis; ++sx) {
                        const float px = x + (sx + 0.5f) / kMouthSamplesPerAxis;
                        const float py = y + (sy + 0.5f) / kMouthSamplesPerAxis;
                        if (px < left || px > right) {
                            continue;
                        }
                        const float nx = (px - left) / mouth_width;
                        const float curve = MouthCurve(nx);
                        const float top_edge = center_y - edge_gap * 0.5f;
                        const float bottom_edge = center_y + edge_gap * 0.5f;
                        float top = top_edge - (open_height * 0.5f - edge_gap * 0.5f) * curve;
                        float bottom = bottom_edge + (open_height * 0.5f - edge_gap * 0.5f) * curve;
                        const float tilt_offset = (nx - 0.5f) * tilt;
                        top += tilt_offset;
                        bottom += tilt_offset;
                        if (mouth_expression_ == MouthExpression::kAngry) {
                            top += (0.5f - curve) * canvas_height * 0.08f;
                            bottom -= curve * canvas_height * 0.04f;
                        } else if (mouth_expression_ == MouthExpression::kLaugh ||
                                   mouth_expression_ == MouthExpression::kFunny) {
                            bottom += curve * canvas_height * 0.08f;
                        }
                        if (py >= top && py <= bottom) {
                            ++fill_count;
                            if (py - top < border || bottom - py < border ||
                                px - left < border || right - px < border) {
                                ++border_count;
                            } else if (show_teeth && py - top < open_height * 0.22f &&
                                       nx > 0.14f && nx < 0.86f) {
                                ++highlight_count;
                            } else if (show_tongue && nx > 0.22f && nx < 0.78f) {
                                const float tx = (nx - 0.22f) / 0.56f;
                                const float tongue_curve = MouthCurve(tx);
                                const float tongue_top = bottom - open_height * (0.30f + 0.12f * tongue_curve);
                                if (py > tongue_top) {
                                    ++tongue_count;
                                }
                            }
                            if (show_highlight && curve > 0.4f && py - top > border &&
                                py - top < border + 2.5f) {
                                ++highlight_count;
                            }
                        }
                    }
                }

                if (border_count > 0) {
                    lv_canvas_set_px(mouth_canvas_, x, y, accent, CoverageToOpa(border_count));
                } else if (highlight_count > 0) {
                    lv_canvas_set_px(mouth_canvas_, x, y, highlight, CoverageToOpa(highlight_count));
                } else if (tongue_count > 0) {
                    lv_canvas_set_px(mouth_canvas_, x, y, tongue, CoverageToOpa(tongue_count));
                } else if (fill_count > 0) {
                    lv_canvas_set_px(mouth_canvas_, x, y, inner, CoverageToOpa(fill_count));
                }
            }
        }
        if (mouth_expression_ == MouthExpression::kDelicious ||
            mouth_expression_ == MouthExpression::kSilly) {
            DrawSoftOval(mouth_canvas_, canvas_width, canvas_height,
                         mouth_center_x + mouth_width * 0.18f,
                         mouth_center_y + open_height * 0.35f, mouth_width * 0.13f,
                         open_height * 0.24f, tongue);
            DrawCurvedStroke(mouth_canvas_, canvas_width, canvas_height,
                             mouth_center_x + mouth_width * 0.10f,
                             mouth_center_x + mouth_width * 0.26f,
                             mouth_center_y + open_height * 0.32f,
                             mouth_center_y + open_height * 0.42f,
                             mouth_center_y + open_height * 0.36f, 1.6f, inner);
        }
    } else {
        if (mouth_expression_ == MouthExpression::kLoving) {
            const float cx = canvas_width * 0.50f;
            const float cy = canvas_height * 0.43f;
            const float r = canvas_height * 0.085f;
            DrawSoftCircle(mouth_canvas_, canvas_width, canvas_height, cx - r * 0.75f,
                           cy - r * 0.12f, r, accent);
            DrawSoftCircle(mouth_canvas_, canvas_width, canvas_height, cx + r * 0.75f,
                           cy - r * 0.12f, r, accent);
            DrawSoftOval(mouth_canvas_, canvas_width, canvas_height, cx, cy + r * 0.52f,
                         r * 1.15f, r * 0.92f, accent);
            DrawSoftCircle(mouth_canvas_, canvas_width, canvas_height, cx + r * 1.0f,
                           cy - r * 0.62f, 1.3f, highlight);
            lv_display_enable_invalidation(display, true);
            lv_obj_align(mouth_canvas_, LV_ALIGN_CENTER, 0, mouth_y);
            lv_obj_invalidate(mouth_canvas_);
            return;
        }

        if (mouth_expression_ == MouthExpression::kKissy) {
            const float cx = canvas_width * 0.5f;
            const float cy = canvas_height * 0.44f;
            const float rx = canvas_width * 0.075f;
            const float ry = canvas_height * 0.075f;
            for (int y = 0; y < canvas_height; ++y) {
                for (int x = 0; x < canvas_width; ++x) {
                    int upper_count = 0;
                    int lower_count = 0;
                    for (int sy = 0; sy < kMouthSamplesPerAxis; ++sy) {
                        for (int sx = 0; sx < kMouthSamplesPerAxis; ++sx) {
                            const float px = x + (sx + 0.5f) / kMouthSamplesPerAxis;
                            const float py = y + (sy + 0.5f) / kMouthSamplesPerAxis;
                            if (OvalValue(px, py, cx, cy - ry * 0.45f, rx, ry * 0.68f) <= 1.0f) {
                                ++upper_count;
                            }
                            if (OvalValue(px, py, cx, cy + ry * 0.45f, rx, ry * 0.68f) <= 1.0f) {
                                ++lower_count;
                            }
                        }
                    }
                    const int coverage = upper_count + lower_count;
                    if (coverage > 0) {
                        lv_canvas_set_px(mouth_canvas_, x, y, accent, CoverageToOpa(coverage));
                    }
                }
            }
            DrawSoftCircle(mouth_canvas_, canvas_width, canvas_height, cx + rx * 1.8f,
                           cy - ry * 1.0f, 1.5f, highlight);
            lv_display_enable_invalidation(display, true);
            lv_obj_align(mouth_canvas_, LV_ALIGN_CENTER, 0, mouth_y);
            lv_obj_invalidate(mouth_canvas_);
            return;
        }

        if (mouth_expression_ == MouthExpression::kCrying) {
            const float mouth_width = canvas_width * 0.62f;
            const float left = (canvas_width - mouth_width) * 0.5f;
            const float right = left + mouth_width;
            const float center_y = canvas_height * 0.46f;
            DrawCurvedStroke(mouth_canvas_, canvas_width, canvas_height, left, right,
                             center_y + canvas_height * 0.11f,
                             center_y - canvas_height * 0.18f,
                             center_y + canvas_height * 0.11f, 5.0f, inner);
            DrawCurvedStroke(mouth_canvas_, canvas_width, canvas_height, left, right,
                             center_y + canvas_height * 0.09f,
                             center_y - canvas_height * 0.20f,
                             center_y + canvas_height * 0.09f, 3.7f, accent);
            DrawSoftOval(mouth_canvas_, canvas_width, canvas_height, right - mouth_width * 0.04f,
                         center_y + canvas_height * 0.20f, 2.4f, 4.4f, accent);
            lv_display_enable_invalidation(display, true);
            lv_obj_align(mouth_canvas_, LV_ALIGN_CENTER, 0, mouth_y);
            lv_obj_invalidate(mouth_canvas_);
            return;
        }

        if (mouth_expression_ == MouthExpression::kThinking) {
            const float cx = canvas_width * 0.47f;
            const float cy = canvas_height * 0.44f;
            DrawSoftCircle(mouth_canvas_, canvas_width, canvas_height, cx - 7.0f, cy, 2.4f,
                           accent);
            DrawSoftCircle(mouth_canvas_, canvas_width, canvas_height, cx, cy - 1.2f, 2.7f,
                           accent);
            DrawCurvedStroke(mouth_canvas_, canvas_width, canvas_height, cx + 4.0f,
                             cx + 21.0f, cy - 1.0f, cy + 5.0f, cy - 2.0f, 3.4f,
                             accent);
            lv_display_enable_invalidation(display, true);
            lv_obj_align(mouth_canvas_, LV_ALIGN_CENTER, 0, mouth_y);
            lv_obj_invalidate(mouth_canvas_);
            return;
        }

        if (mouth_expression_ == MouthExpression::kConfused) {
            const float cy = canvas_height * 0.45f;
            const float left = canvas_width * 0.28f;
            const float mid = canvas_width * 0.50f;
            const float right = canvas_width * 0.72f;
            DrawCurvedStroke(mouth_canvas_, canvas_width, canvas_height, left, mid,
                             cy + canvas_height * 0.05f, cy - canvas_height * 0.10f,
                             cy + canvas_height * 0.03f, 3.8f, accent);
            DrawCurvedStroke(mouth_canvas_, canvas_width, canvas_height, mid, right,
                             cy + canvas_height * 0.03f, cy + canvas_height * 0.15f,
                             cy - canvas_height * 0.08f, 3.8f, accent);
            lv_display_enable_invalidation(display, true);
            lv_obj_align(mouth_canvas_, LV_ALIGN_CENTER, 0, mouth_y);
            lv_obj_invalidate(mouth_canvas_);
            return;
        }

        if (mouth_expression_ == MouthExpression::kAngry) {
            const float mouth_width = canvas_width * 0.58f;
            const float left = (canvas_width - mouth_width) * 0.5f;
            const float right = left + mouth_width;
            const float top = canvas_height * 0.40f;
            const float bottom = canvas_height * 0.54f;
            const float border = 3.0f;
            for (int y = 0; y < canvas_height; ++y) {
                for (int x = 0; x < canvas_width; ++x) {
                    int border_count = 0;
                    int tooth_count = 0;
                    int separator_count = 0;
                    for (int sy = 0; sy < kMouthSamplesPerAxis; ++sy) {
                        for (int sx = 0; sx < kMouthSamplesPerAxis; ++sx) {
                            const float px = x + (sx + 0.5f) / kMouthSamplesPerAxis;
                            const float py = y + (sy + 0.5f) / kMouthSamplesPerAxis;
                            if (px < left || px > right || py < top || py > bottom) {
                                continue;
                            }
                            if (px - left < border || right - px < border ||
                                py - top < border || bottom - py < border) {
                                ++border_count;
                            } else {
                                const float slot = (px - left) / (mouth_width / 4.0f);
                                const float frac = slot - static_cast<int>(slot);
                                if (frac < 0.05f || frac > 0.95f) {
                                    ++separator_count;
                                } else {
                                    ++tooth_count;
                                }
                            }
                        }
                    }
                    if (border_count > 0) {
                        lv_canvas_set_px(mouth_canvas_, x, y, accent, CoverageToOpa(border_count));
                    } else if (separator_count > 0) {
                        lv_canvas_set_px(mouth_canvas_, x, y, inner, CoverageToOpa(separator_count));
                    } else if (tooth_count > 0) {
                        lv_canvas_set_px(mouth_canvas_, x, y, highlight, CoverageToOpa(tooth_count));
                    }
                }
            }
            lv_display_enable_invalidation(display, true);
            lv_obj_align(mouth_canvas_, LV_ALIGN_CENTER, 0, mouth_y);
            lv_obj_invalidate(mouth_canvas_);
            return;
        }

        const float mouth_width = (mouth_expression_ == MouthExpression::kConfident ||
                                   mouth_expression_ == MouthExpression::kWinking ||
                                   mouth_expression_ == MouthExpression::kCool ||
                                   mouth_expression_ == MouthExpression::kEmbarrassed)
                                      ? canvas_width * 0.58f
                                      : canvas_width * 0.66f;
        const float left = (canvas_width - mouth_width) * 0.5f;
        const float right = left + mouth_width;
        const float center_y = canvas_height * 0.44f;
        float end_y = center_y - canvas_height * 0.06f;
        float mid_y = center_y + canvas_height * 0.18f;
        float right_end_y = end_y;
        float mouth_center_shift = 0.0f;
        float thickness = 4.2f;
        if (mouth_expression_ == MouthExpression::kSad) {
            end_y = center_y + canvas_height * 0.12f;
            right_end_y = end_y;
            mid_y = center_y - canvas_height * 0.16f;
        } else if (mouth_expression_ == MouthExpression::kSleepy) {
            end_y = center_y;
            right_end_y = center_y + canvas_height * 0.03f;
            mid_y = center_y + canvas_height * 0.06f;
            thickness = 3.5f;
            mouth_center_shift = -canvas_width * 0.02f;
        } else if (mouth_expression_ == MouthExpression::kConfident) {
            end_y = center_y + canvas_height * 0.08f;
            right_end_y = center_y - canvas_height * 0.10f;
            mid_y = center_y;
            thickness = 4.0f;
            mouth_center_shift = canvas_width * 0.03f;
        } else if (mouth_expression_ == MouthExpression::kWinking) {
            end_y = center_y + canvas_height * 0.02f;
            right_end_y = center_y - canvas_height * 0.10f;
            mid_y = center_y + canvas_height * 0.10f;
            thickness = 4.1f;
            mouth_center_shift = canvas_width * 0.04f;
        } else if (mouth_expression_ == MouthExpression::kEmbarrassed) {
            end_y = center_y - canvas_height * 0.02f;
            right_end_y = center_y - canvas_height * 0.01f;
            mid_y = center_y + canvas_height * 0.07f;
            thickness = 3.5f;
            mouth_center_shift = -canvas_width * 0.02f;
        } else if (mouth_expression_ == MouthExpression::kCool) {
            end_y = center_y;
            right_end_y = center_y - canvas_height * 0.03f;
            mid_y = center_y - canvas_height * 0.01f;
            thickness = 3.8f;
        } else if (mouth_expression_ == MouthExpression::kRelaxed) {
            end_y = center_y - canvas_height * 0.02f;
            right_end_y = end_y;
            mid_y = center_y + canvas_height * 0.12f;
            thickness = 3.8f;
        } else if (mouth_expression_ == MouthExpression::kAngry) {
            end_y = center_y + canvas_height * 0.06f;
            right_end_y = end_y;
            mid_y = center_y;
            thickness = 4.0f;
        } else if (mouth_expression_ == MouthExpression::kNeutral) {
            end_y = center_y;
            right_end_y = end_y;
            mid_y = center_y;
            thickness = 3.5f;
        }

        if (mouth_expression_ != MouthExpression::kNeutral &&
            mouth_expression_ != MouthExpression::kCool &&
            mouth_expression_ != MouthExpression::kSleepy) {
            DrawCurvedStroke(mouth_canvas_, canvas_width, canvas_height,
                             left + mouth_center_shift, right + mouth_center_shift,
                             end_y + canvas_height * 0.025f,
                             mid_y + canvas_height * 0.025f,
                             right_end_y + canvas_height * 0.025f, thickness + 2.0f, inner);
        }

        for (int y = 0; y < canvas_height; ++y) {
            for (int x = 0; x < canvas_width; ++x) {
                int coverage = 0;
                for (int sy = 0; sy < kMouthSamplesPerAxis; ++sy) {
                    for (int sx = 0; sx < kMouthSamplesPerAxis; ++sx) {
                        const float px = x + (sx + 0.5f) / kMouthSamplesPerAxis;
                        const float py = y + (sy + 0.5f) / kMouthSamplesPerAxis;
                        if (px < left + mouth_center_shift || px > right + mouth_center_shift) {
                            continue;
                        }
                        const float nx = (px - left - mouth_center_shift) / mouth_width;
                        const float linear_y = end_y + (right_end_y - end_y) * nx;
                        const float curve_y = linear_y + (mid_y - linear_y) * MouthCurve(nx);
                        if (py > curve_y - thickness * 0.5f &&
                            py < curve_y + thickness * 0.5f) {
                            ++coverage;
                        }
                    }
                }
                if (coverage > 0) {
                    lv_canvas_set_px(mouth_canvas_, x, y, accent, CoverageToOpa(coverage));
                }
            }
        }
        if (mouth_expression_ == MouthExpression::kSmile ||
            mouth_expression_ == MouthExpression::kRelaxed ||
            mouth_expression_ == MouthExpression::kConfident ||
            mouth_expression_ == MouthExpression::kWinking) {
            DrawSoftCircle(mouth_canvas_, canvas_width, canvas_height,
                           left + mouth_center_shift + mouth_width * 0.02f, end_y, 2.2f,
                           accent);
            DrawSoftCircle(mouth_canvas_, canvas_width, canvas_height,
                           right + mouth_center_shift - mouth_width * 0.02f, right_end_y, 2.2f,
                           accent);
        }
        if (mouth_expression_ == MouthExpression::kSleepy) {
            DrawSoftOval(mouth_canvas_, canvas_width, canvas_height,
                         right + mouth_center_shift + canvas_width * 0.08f,
                         center_y - canvas_height * 0.10f, 3.4f, 2.3f, accent);
        } else if (mouth_expression_ == MouthExpression::kEmbarrassed) {
            DrawSoftCircle(mouth_canvas_, canvas_width, canvas_height,
                           right + mouth_center_shift + canvas_width * 0.04f,
                           center_y - canvas_height * 0.05f, 1.8f, highlight);
        }
    }

    lv_display_enable_invalidation(display, true);
    lv_obj_align(mouth_canvas_, LV_ALIGN_CENTER, 0, mouth_y);
    lv_obj_invalidate(mouth_canvas_);
}

void OttoEmojiDisplay::SetMouthLayerHidden(bool hidden) {
    SetObjectHidden(mouth_layer_, hidden);
}

void OttoEmojiDisplay::MoveMouthToForeground() {
    if (mouth_layer_ != nullptr) {
        lv_obj_move_foreground(mouth_layer_);
    }
}

void OttoEmojiDisplay::SetupPreviewImage() {
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        ESP_LOGW(TAG,
                 "SetupPreviewImage called but preview_image_ is nullptr (UI not initialized yet)");
        return;
    }
    lv_obj_set_size(preview_image_, width_, height_);
}

void OttoEmojiDisplay::InitializeOttoEmojis() {
    ESP_LOGI(TAG, "Otto表情初始化将由Assets系统处理");
    // 表情初始化已移至assets系统,通过DEFAULT_EMOJI_COLLECTION=otto-gif配置
    // assets.cc会从assets分区加载GIF表情并设置到theme
    // Note: Default emotion is now set in SetupUI() after LVGL objects are created
}

LV_FONT_DECLARE(OTTO_ICON_FONT);
void OttoEmojiDisplay::SetStatus(const char* status) {
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    if (!status) {
        ESP_LOGE(TAG, "SetStatus: status is nullptr");
        return;
    }

    bool new_speaking_status = strcmp(status, Lang::Strings::SPEAKING) == 0;
    if (speaking_status_ != new_speaking_status) {
        speaking_status_ = new_speaking_status;
        RefreshOttoGifEmotion();
    }

    DisplayLockGuard lock(this);
    if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        lv_obj_set_style_text_font(status_label_, &OTTO_ICON_FONT, 0);
        lv_label_set_text(status_label_, "\xEF\x84\xB0");  // U+F130 麦克风图标
        lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        lv_obj_set_style_text_font(status_label_, &OTTO_ICON_FONT, 0);
        lv_label_set_text(status_label_, "\xEF\x80\xA8");  // U+F028 说话图标
        lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    } else if (strcmp(status, Lang::Strings::CONNECTING) == 0) {
        lv_obj_set_style_text_font(status_label_, &OTTO_ICON_FONT, 0);
        lv_label_set_text(status_label_, "\xEF\x83\x81");  // U+F0c1 连接图标
        lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        lv_obj_set_style_text_font(status_label_, text_font, 0);
        lv_label_set_text(status_label_, "");
        lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(network_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_set_style_text_font(status_label_, text_font, 0);
    lv_label_set_text(status_label_, status);
}

void OttoEmojiDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) {
        return;
    }

    current_emotion_ = emotion;
    RefreshOttoGifEmotion();
}

void OttoEmojiDisplay::RefreshOttoGifEmotion() {
    if (speaking_status_) {
        std::string talking_emotion = current_emotion_ + "_talking";
        SpiLcdDisplay::SetEmotion(talking_emotion.c_str());
        return;
    }
    SpiLcdDisplay::SetEmotion(current_emotion_.c_str());
}

void OttoEmojiDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        ESP_LOGE(TAG, "Preview image is not initialized");
        return;
    }

    if (image == nullptr) {
        esp_timer_stop(preview_timer_);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        preview_image_cached_.reset();
        if (gif_controller_) {
            gif_controller_->Start();
        }
        return;
    }

    preview_image_cached_ = std::move(image);
    auto img_dsc = preview_image_cached_->image_dsc();
    // 设置图片源并显示预览图片
    lv_image_set_src(preview_image_, img_dsc);
    lv_image_set_rotation(preview_image_, 900);
    if (img_dsc->header.w > 0 && img_dsc->header.h > 0) {
        // zoom factor 1.0
        lv_image_set_scale(preview_image_, 256 * width_ / img_dsc->header.w);
    }

    // Hide emoji_box_
    if (gif_controller_) {
        gif_controller_->Stop();
    }
    lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    esp_timer_stop(preview_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, PREVIEW_IMAGE_DURATION_MS * 1000));
}
