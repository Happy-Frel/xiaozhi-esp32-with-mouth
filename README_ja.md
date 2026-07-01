# xiaozhi-esp32-with-mouth

このリポジトリは [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) をベースにした Otto ロボット向けの改良版です。

主な変更点は次の通りです。

- Otto の GIF 表情素材に口を追加
- 会話中は通常 GIF から `_talking` GIF に切り替え
- LVGL canvas によるリアルタイム描画ではなく、GIF 素材を事前に編集する方式を採用
- 音声で開始・キャンセルできるローカル Pomodoro タイマーを追加
- BOOT ボタンのダブルクリックで残り時間を 10 秒間表示

詳しい説明とビルド方法は [README.md](README.md) と [README_zh.md](README_zh.md) を参照してください。

このプロジェクトは元の `xiaozhi-esp32` と同じく MIT License に従います。
