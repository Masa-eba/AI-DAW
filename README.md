# AI-DAW

AI-DAWは、C++20、JUCE、CMakeで開発しているmacOS向けのネイティブDTMアプリです。

現在のDTM版MVPでは、複数のAudio/MIDIトラック、BPM、タイムライン、メトロノーム、画面鍵盤、簡易シンセ、録音、ミックスWAV書き出し、プロジェクト保存の基礎を実装しています。

## 機能

- 複数Audioトラック
- 複数MIDIトラック
- トラック名変更
- トラック複製
- トラック上下移動
- 新規プロジェクト作成
- AudioトラックへのWAV / AIFF / AIF / MP3読み込み
- 全トラック同期再生 / 一時停止 / 停止 / シーク
- BPM設定
- 小節 / 拍グリッドつきタイムライン
- 再生位置線
- Timeline Zoom
- メトロノーム
- トラックごとのVolume / Pan / Mute / Solo / Record Arm
- マスター音量
- ループ再生
- 選択Clipの範囲ループ
- タイムライン上のループ範囲表示
- Undo / Redo
- 16分グリッドへのスナップ移動
- 選択Clipの16分ナッジ移動
- 再生位置の拍 / 小節移動
- AudioClipの選択 / 複製 / 削除
- AudioClipのSplit
- AudioClipの左右端トリム
- AudioClipのFade In / Fade Out
- AudioClipのClip Gain調整
- AudioClipのドラッグ移動
- AudioClipのAudioトラック間移動
- MidiClipの選択 / 複製 / 削除
- MidiClipのドラッグ移動
- MidiClipのMIDIトラック間移動
- MidiClipの右端トリム
- MidiClipの16分クオンタイズ
- MidiClipのオクターブ上下移調
- MidiClipのVelocity上下調整
- 画面鍵盤によるMIDI発音
- PCキーボードによるMIDI発音
- 外部MIDI入力デバイス選択
- 内蔵サイン波シンセ
- Panic / All Notes Off
- MIDI録音の基礎
- AI Chordsによる簡易コード進行生成
- マイク録音の基礎
- 全トラックのミックスWAV書き出し
- `.aidaw`プロジェクト保存 / 読み込み
- プロジェクト保存時のAudio素材コピーと相対パス保存

## 技術スタック

- C++20
- JUCE
- CMake
- macOS
- Apple Silicon

## 必要環境

- macOS
- Xcode Command Line Tools
- CMake 3.22以上
- Git

## macOS権限

マイク録音にはmacOSのマイク権限が必要です。初回録音時または起動時に権限確認が表示された場合は許可してください。

## クローン

JUCEはGit Submoduleとして管理しています。

```bash
git clone --recursive git@github.com:Masa-eba/AI-DAW.git
cd AI-DAW
```

`--recursive`なしでクローンした場合は、以下を実行してください。

```bash
git submodule update --init --recursive
```

## ビルド

```bash
cmake -S . -B build
cmake --build build
```

## 起動

```bash
open build/MiniDAW_artefacts/AI-DAW.app
```

## MIDI確認方法

- 外部MIDIキーボードを接続し、`MIDI Input`から選択する
- 画面下部の鍵盤をクリックする
- PCキーボードで演奏する
  - `A W S E D F T G Y H U J K`
  - Cから1オクターブ分の白鍵 / 黒鍵に対応

## 基本操作

1. `New`で新規プロジェクトを作成
2. `+ Audio Track`でAudioトラックを追加
3. トラックを選択して`Import Audio`から音声ファイルを読み込む
4. `+ MIDI Track`でMIDIトラックを追加
5. `Rename`で選択中トラック名を変更
6. `Dup Track`で選択中トラックを複製
7. `Option + Shift + ↑ / ↓`で選択中トラックを上下へ移動
8. MIDIトラックを選択して`R`を有効化
9. 画面鍵盤または外部MIDIキーボードで発音
10. `Record`で録音
11. AudioClipを掴んでタイムライン上で移動
12. AudioClipの左右端をドラッグして非破壊トリム
13. 再生位置線をAudioClip内に置いて`Split`で分割
14. `Duplicate Clip` / `Delete Clip`でAudioClipを編集
15. `Fade In` / `Fade Out`で1秒フェードを適用
16. `Command + = / -`で選択中AudioClipのClip Gainを上下
17. `AI Chords`で選択中MIDIトラックへコード進行を生成
18. MidiClipを掴んでタイムライン上または別MIDIトラックへ移動
19. MidiClipの右端をドラッグして長さを変更
20. `Duplicate Clip` / `Delete Clip`でMidiClipを編集
21. `Quantize`で選択中MidiClipを16分グリッドへ揃える
22. `Command + ↑ / ↓`で選択中MidiClipをオクターブ上下に移調
23. `Command + ] / [`で選択中MidiClipのVelocityを上下
24. `Zoom`でタイムラインの横方向表示倍率を調整
25. `Command + ← / →`で選択中Clipを16分単位で左右へ移動
26. `Option + ← / →`で再生位置を前後の拍へ移動
27. `Option + Shift + ← / →`で再生位置を前後の小節へ移動
28. `M / S / Volume / Pan`でトラックを調整
29. `Loop Clip`で選択中AudioClip / MidiClipの範囲を繰り返し再生
30. `Loop`でプロジェクト長の範囲を繰り返し再生
31. `Panic`または`Esc`で鳴りっぱなしのMIDIノートを停止
32. `Export Mix`でミックス結果を書き出す

## プロジェクト保存

`.aidaw`保存時、AudioClipが参照している音声素材はプロジェクトファイルと同じ階層の`Audio/`フォルダへコピーされます。
プロジェクトJSON内には`Audio/filename.wav`のような相対パスを保存するため、プロジェクトフォルダ単位で移動しやすくなっています。

## ショートカット

- `Space`: 再生 / 一時停止
- `Esc`: Panic / All Notes Off
- `Command + Z`: Undo
- `Command + Shift + Z`: Redo
- `Command + ← / →`: 選択中AudioClip / MidiClipを16分単位で左右へ移動
- `Option + ← / →`: 再生位置を前後の拍へ移動
- `Option + Shift + ← / →`: 再生位置を前後の小節へ移動
- `Option + Shift + ↑ / ↓`: 選択中トラックを同種トラック内で上下へ移動
- `Command + D`: 選択中AudioClip / MidiClipを複製
- `Command + E`: 選択中AudioClipを再生位置でSplit
- `Command + = / -`: 選択中AudioClipのClip Gainを上下
- `Command + K`: 選択中MidiClipを16分クオンタイズ
- `Command + ↑ / ↓`: 選択中MidiClipを1オクターブ上下に移調
- `Command + ] / [`：選択中MidiClipのVelocityを上下
- `Delete` / `Backspace`: 選択中AudioClip / MidiClipを削除

## ディレクトリ構成

```text
AI-DAW/
├── CMakeLists.txt
├── README.md
├── JUCE/
└── Source/
    ├── Main.cpp
    ├── MainComponent.h
    ├── MainComponent.cpp
    ├── AudioEngine.h
    ├── AudioEngine.cpp
    ├── Core/
    │   ├── ProjectModel.h
    │   ├── ProjectModel.cpp
    │   ├── TempoMap.h
    │   └── TempoMap.cpp
    ├── Audio/
    │   ├── AudioTrack.h
    │   ├── AudioTrack.cpp
    │   ├── AudioClip.h
    │   ├── AudioRecorder.h
    │   ├── AudioRecorder.cpp
    │   ├── Metronome.h
    │   └── Metronome.cpp
    ├── Midi/
    │   ├── MidiTrack.h
    │   ├── MidiTrack.cpp
    │   ├── MidiClip.h
    │   ├── MidiInputManager.h
    │   ├── MidiInputManager.cpp
    │   ├── SimpleSynth.h
    │   └── SimpleSynth.cpp
    ├── UI/
    │   ├── TimelineComponent.h
    │   └── TimelineComponent.cpp
    ├── WaveformComponent.h
    ├── WaveformComponent.cpp
    ├── TimeFormatter.h
    └── TimeFormatter.cpp
```

## 主要クラス

- `ProjectModel`
  - BPM、Audio/MIDIトラック、プロジェクト保存状態を管理

- `AudioEngine`
  - `AudioIODeviceCallback`として出力、入力、録音、ミックスを担当
  - 全トラックを共通トランスポート位置で同期

- `AudioTrack`
  - AudioClip、トラック音量、Mute、Solo、Record Armを保持

- `MidiTrack`
  - MidiClip、トラック音量、Mute、Solo、Record Armを保持

- `SimpleSynth`
  - 16ボイスの簡易サイン波シンセ

- `Metronome`
  - オーディオコールバック内でBPM同期クリックを生成

- `AudioRecorder`
  - `AudioFormatWriter::ThreadedWriter`でマイク録音を書き込み

- `TimelineComponent`
  - 小節線、拍線、Audio/MIDIクリップ、再生位置線を描画
  - AudioClip / MidiClipのドラッグ移動、ファイルドロップ、スナップを担当

## 現在の制限

- ピアノロール編集は未実装
- AudioClip編集はSplit、左右端トリム、複製、削除、1秒フェード、Clip Gainのみ対応
- MidiClip編集は移動、右端トリム、複製、削除、16分クオンタイズ、オクターブ移調、Velocity調整のみ対応
- Audioタイムストレッチは未実装
- VST / AUプラグインには未対応
- MIDI CC編集、クオンタイズは未実装
- AI Chordsはローカルの簡易コード進行生成で、外部AI APIは未接続
- Audioファイルは現在メモリへ読み込むため、長時間ファイルには不向き
- マイク録音とMIDI録音は基礎実装で、同時録音や詳細編集は今後の対象

## 今後の予定

- ピアノロール
- MIDIノート編集
- AudioClip編集
- 複数クリップ編集
- エフェクト
- Audio-to-MIDI
- AIコード提案
- AI伴奏生成
