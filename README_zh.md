# 带嘴巴表情与番茄钟的小智 Otto 机器人

本仓库基于开源项目 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 修改，面向 Otto 机器人形态增加了两个主要功能：带嘴巴的 GIF 表情，以及可通过语音控制的番茄钟。

## 改进效果

- Otto 表情从“只有整张脸变化”改为“表情中带有嘴巴反馈”
- 说话状态会切换到对应的 `_talking` GIF，让机器人看起来像在开口回应
- 用户可以通过语音设置、查询、取消番茄钟
- 双击 BOOT 按键可以临时查看剩余倒计时，显示 10 秒后自动恢复正常表情
- 倒计时结束时屏幕提示并播放提示音

## 技术原理

### 嘴巴表情

原开源 Otto 表情没有独立嘴巴设计。本版本的最终方案是修改 GIF 素材，把嘴巴直接绘入 Otto 表情帧中，并为说话状态制作 talking 表情。

曾尝试过运行时使用 LVGL canvas 绘制嘴巴，但复杂图形会造成屏幕刷新卡顿，因此最终放弃运行时叠加路线，改为更轻量的 GIF 素材方案。

运行时逻辑位于 `main/boards/otto-robot/otto_emoji_display.cc`：当设备处于说话状态时，显示层会把当前表情切换为 `current_emotion_ + "_talking"`；说话结束后恢复普通表情。

### 番茄钟

番茄钟由 `PomodoroTimer` 单例维护本地状态，使用 ESP-IDF 定时器计算剩余时间。MCP 层注册了以下工具，让大模型可以通过对话调用：

| 工具 | 作用 |
| --- | --- |
| `self.pomodoro.start` | 开始本地番茄钟 |
| `self.pomodoro.cancel` | 取消当前番茄钟 |
| `self.pomodoro.get_status` | 查询剩余时间 |
| `self.pomodoro.show` | 在屏幕上显示倒计时 10 秒 |

BOOT 按键双击只负责显示倒计时，不负责取消。这样语音语义和按钮操作边界清晰，交互也更符合桌面机器人使用场景。

## 构建说明

推荐查看本仓库中的构建记录：

- [docs/otto_build_record.md](docs/otto_build_record.md)
- [scripts/otto_build_com10.ps1](scripts/otto_build_com10.ps1)

已验证环境：

```text
ESP-IDF: v5.5.2
Target: esp32s3
Board: otto-robot
Flash port: COM10
```

在 Windows 下，如果源码路径包含空格或括号，建议用短路径镜像构建：

```powershell
.\scripts\otto_build_com10.ps1 -Flash
```

## 主要文件

```text
main/pomodoro_timer.cc / .h             番茄钟本地计时模块
main/mcp_server.cc                      番茄钟 MCP 工具注册
main/boards/otto-robot/otto_robot.cc    BOOT 双击触发显示倒计时
main/boards/otto-robot/otto_emoji_display.cc
                                        GIF 表情切换与番茄钟覆盖层
managed_components/txp666__otto-emoji-gif-component/gifs/
                                        带嘴巴的 Otto GIF 表情素材
```

## 上游基础能力

本仓库保留小智原有能力，包括语音唤醒、语音对话、LCD 表情、MCP 设备控制、Otto 动作控制、多开发板支持等。本仓库新增内容主要集中在 Otto 机器人屏幕表现和学习陪伴交互。

## 许可证

本仓库基于 `xiaozhi-esp32` 修改，继续遵循 MIT License。原项目和第三方组件版权归各自作者所有。
