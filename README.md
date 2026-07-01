# xiaozhi-esp32-with-mouth

这是一个基于开源项目 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 二次开发的 Otto 小智机器人版本。原项目已经具备语音唤醒、语音对话、LCD 表情、MCP 设备控制和 Otto 机器人动作能力；本仓库在此基础上重点增强了机器人屏幕表现和学习陪伴功能。

## 主要改进

### 1. 带嘴巴的 Otto 表情

原开源 Otto 表情是整张 GIF 表情切换，没有独立嘴巴设计。这个版本重新设计了 Otto 的嘴巴视觉，并将嘴巴直接绘入 GIF 表情帧中。

最终采用的是 GIF 素材改造方案，而不是运行时 canvas 叠加绘制。开发中曾尝试过在 LVGL canvas 上实时绘制嘴巴，但复杂图形会让小屏幕刷新变卡顿，所以改为离线修改 GIF 素材：普通状态使用正常表情 GIF，说话状态切换到对应的 `_talking` GIF。

相关实现：

- `managed_components/txp666__otto-emoji-gif-component/gifs/`：带嘴巴的 Otto GIF 表情素材
- `main/boards/otto-robot/otto_emoji_display.cc`：根据说话状态在普通 GIF 和 `_talking` GIF 之间切换

### 2. 自定义番茄钟

新增本地番茄钟功能，适合把小智作为桌面学习陪伴机器人使用。

效果：

- 通过语音让小智设置番茄钟，例如“帮我设置一个 25 分钟的番茄钟”
- 通过语音取消番茄钟
- 双击 BOOT 按键时，屏幕临时切换到倒计时界面
- 倒计时显示 10 秒后自动回到正常表情
- 倒计时结束时播放提示音并显示完成状态

相关实现：

- `main/pomodoro_timer.cc` / `main/pomodoro_timer.h`：本地计时器、剩余时间计算、完成回调
- `main/mcp_server.cc`：注册 `self.pomodoro.start`、`self.pomodoro.cancel`、`self.pomodoro.get_status`、`self.pomodoro.show`
- `main/boards/otto-robot/otto_robot.cc`：BOOT 双击触发倒计时显示
- `main/boards/otto-robot/otto_emoji_display.cc`：番茄钟覆盖层、倒计时文字和进度条显示

## 技术思路

本项目尽量不改变原有主状态机和机器人外形，只在显示层、MCP 工具层和本地计时模块之间增加轻量协作。

嘴巴功能的核心是素材层改造：把嘴巴烘焙进 GIF 帧，运行时只负责根据 `speaking_status_` 切换普通表情或 talking 表情。这样比实时绘制更稳定，也更适合 ESP32-S3 上的 240 x 240 小屏幕。

番茄钟功能的核心是计时和显示解耦：`PomodoroTimer` 使用 ESP-IDF 定时器维护本地倒计时，显示层只在需要时覆盖当前表情。双击按键只负责显示倒计时，取消计时交给语音指令，避免按钮逻辑变复杂。

## 构建与烧录

本仓库主要面向 `otto-robot` 板型。当前记录的 Windows 构建方式见：

- [docs/otto_build_record.md](docs/otto_build_record.md)
- [scripts/otto_build_com10.ps1](scripts/otto_build_com10.ps1)

本机验证过的组合：

```text
ESP-IDF: v5.5.2
Target: esp32s3
Board: otto-robot
Flash port: COM10
```

如果源码路径中有空格或括号，建议使用 `D:\OttoBuild\xz_com10_build` 这类短路径镜像构建，避免 ESP-IDF 构建路径解析问题。

```powershell
.\scripts\otto_build_com10.ps1 -Flash
```

## 与上游项目的关系

本仓库保留了 `xiaozhi-esp32` 的基础能力，包括：

- 离线语音唤醒
- 流式 ASR + LLM + TTS 语音对话
- LCD / OLED 表情显示
- 设备端 MCP 控制
- Otto 机器人动作控制
- 多开发板支持

本仓库的重点是 Otto 机器人方向的个性化改进：带嘴巴表情、说话状态 GIF 切换、番茄钟学习陪伴交互。

## 目录说明

```text
main/boards/otto-robot/                 Otto 机器人板级实现
main/pomodoro_timer.*                   番茄钟本地计时模块
main/mcp_server.cc                      MCP 工具注册
managed_components/txp666__otto-emoji-gif-component/
                                        Otto GIF 表情素材
docs/otto_build_record.md               本机已验证构建记录
scripts/otto_build_com10.ps1            Windows 构建/烧录脚本
```

## 许可证

本项目基于原 `xiaozhi-esp32` 项目修改，继续遵循仓库中的 MIT License。原项目版权和第三方组件版权归各自作者所有。
