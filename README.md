# AI-DAW

AI-DAWは、C++20、JUCE、CMakeで開発しているmacOS向けのネイティブDTMアプリです。

現在のDTM版MVPでは、複数のAudio/MIDIトラック、BPM、タイムライン、メトロノーム、画面鍵盤、簡易シンセ、録音、ミックスWAV書き出し、プロジェクト保存の基礎を実装しています。

## 機能

- 複数Audioトラック
- 複数MIDIトラック
- AudioトラックへのWAV / AIFF / AIF / MP3読み込み
- 全トラック同期再生 / 一時停止 / 停止 / シーク
- BPM設定
- 小節 / 拍グリッドつきタイムライン
- 再生位置線
- メトロノーム
- トラックごとのVolume / Mute / Solo / Record Arm
- マスター音量
- ループ再生
- 16分グリッドへのスナップ移動
- AudioClipの選択 / 複製 / 削除
- AudioClipのSplit
- AudioClipのFade In / Fade Out
- AudioClipのドラッグ移動
- AudioClipのAudioトラック間移動
- MidiClipの選択 / 複製 / 削除
- MidiClipのドラッグ移動
- MidiClipのMIDIトラック間移動
- 画面鍵盤によるMIDI発音
- PCキーボードによるMIDI発音
- 外部MIDI入力デバイス選択
- 内蔵サイン波シンセ
- MIDI録音の基礎
- AI Chordsによる簡易コード進行生成
- マイク録音の基礎
- 全トラックのミックスWAV書き出し
- `.aidaw`プロジェクト保存 / 読み込み

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

1. `+ Audio Track`でAudioトラックを追加
2. トラックを選択して`Import Audio`から音声ファイルを読み込む
3. `+ MIDI Track`でMIDIトラックを追加
4. MIDIトラックを選択して`R`を有効化
5. 画面鍵盤または外部MIDIキーボードで発音
6. `Record`で録音
7. AudioClipを掴んでタイムライン上で移動
8. 再生位置線をAudioClip内に置いて`Split`で分割
9. `Duplicate Clip` / `Delete Clip`でAudioClipを編集
10. `Fade In` / `Fade Out`で1秒フェードを適用
11. `AI Chords`で選択中MIDIトラックへコード進行を生成
12. MidiClipを掴んでタイムライン上または別MIDIトラックへ移動
13. `Duplicate Clip` / `Delete Clip`でMidiClipを編集
14. `M / S / Volume`でトラックを調整
15. `Loop`でプロジェクト長の範囲を繰り返し再生
16. `Export Mix`でミックス結果を書き出す

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
- AudioClip編集はSplit、複製、削除、1秒フェードのみ対応
- MidiClip編集は移動、複製、削除のみ対応
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
- Undo / Redo
- エフェクト
- Audio-to-MIDI
- AIコード提案
- AI伴奏生成
