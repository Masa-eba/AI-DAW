# AI-DAW

AI-DAWは、C++20、JUCE、CMakeで開発しているmacOS向けのネイティブDTMアプリです。

現在のDTM版MVPでは、複数のAudio/MIDIトラック、BPM、タイムライン、メトロノーム、画面鍵盤、簡易シンセ、録音、ミックス/Stem WAV書き出し、プロジェクト保存の基礎を実装しています。

## 機能

- 複数Audioトラック
- 複数MIDIトラック
- トラック名変更
- トラック複製と作成トラックの自動選択
- トラック上下移動
- 新規プロジェクト作成
- タイトルへの現在プロジェクト名表示
- AudioトラックへのWAV / AIFF / AIF / MP3読み込み
- 全トラック同期再生 / 一時停止 / 停止 / シーク
- BPM設定
- 小節 / 拍グリッドつきタイムライン
- 再生位置線
- タイムラインマーカー追加 / 前後ジャンプ
- Timeline Zoom
- メトロノーム
- トラックごとのVolume / Pan / Mute / Solo / Record Arm
- 全トラックのMute / Solo一括解除
- マスター音量
- マスターピークホールドメーター
- Monoモニター
- ループ再生
- 選択Clipの範囲ループ
- タイムライン上のループ範囲表示
- Undo / Redo
- 4分 / 8分 / 16分 / 32分グリッドへのスナップ移動
- 選択Clipのグリッド単位ナッジ移動
- 再生位置の拍 / 小節移動
- AudioClipの選択 / 複製 / 削除
- AudioClipの再生位置への複製
- AudioClipの再生位置への移動
- AudioClip Mute
- AudioClip / MidiClipのSplit
- AudioClipの左右端トリム
- AudioClipのFade In / Fade Out
- AudioClipの両端クリック除去用ショートフェード
- AudioClipのFade Clear
- AudioClipのClip Gain調整
- AudioClip Normalize
- AudioClip Reverse
- AudioClipのドラッグ移動
- AudioClipのAudioトラック間移動
- AudioClipのタイムライン波形プレビュー
- MidiClipの選択 / 複製 / 削除
- MidiClipの再生位置への複製
- MidiClipの再生位置への移動
- MidiClip Mute
- MidiClipのドラッグ移動
- MidiClipのMIDIトラック間移動
- MidiClipのタイムラインノートプレビュー
- MidiClipの右端トリム
- MidiClipの16分クオンタイズ
- MidiClipのメジャー / マイナースケール補正
- MidiClipのオクターブ上下移調
- MidiClipのピッチ反転
- MidiClipのVelocity上下調整
- MidiClipのVelocity Humanize
- MidiClipのTiming Humanize
- MidiClip Legato
- 画面鍵盤によるMIDI発音
- PCキーボードによるMIDI発音
- 外部MIDI入力デバイス選択
- 内蔵サイン波シンセ
- Panic / All Notes Off
- MIDI録音の基礎
- AI Chordsによる簡易コード進行生成
- AI Bassによる簡易ベースライン生成
- AI Arpによる簡易アルペジオ生成
- マイク録音の基礎
- 全トラックのミックスWAV書き出し
- 選択トラックのStem WAV書き出し
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
18. `AI Bass`で選択中MIDIトラックへベースラインを生成
19. `AI Arp`で選択中MIDIトラックへアルペジオを生成
20. MidiClipを掴んでタイムライン上または別MIDIトラックへ移動
21. MidiClipの右端をドラッグして長さを変更
22. `Duplicate Clip` / `Delete Clip`でMidiClipを編集
23. `Quantize`で選択中MidiClipを現在のスナップグリッドへ揃える
24. `Command + ↑ / ↓`で選択中MidiClipをオクターブ上下に移調
25. `Command + ] / [`で選択中MidiClipのVelocityを上下
26. `Zoom`でタイムラインの横方向表示倍率を調整
27. `Command + ← / →`で選択中Clipを現在のスナップグリッド単位で左右へ移動
28. `Command + J`で選択中Clipを再生位置へ移動
29. `Option + ← / →`で再生位置を前後の拍へ移動
30. `Option + Shift + ← / →`で再生位置を前後の小節へ移動
31. `Option + M`で現在位置にマーカーを追加
32. `Option + Shift + M`で再生位置付近のマーカー名を変更
33. `Option + Delete`で再生位置付近のマーカーを削除
34. `Option + , / .`で前後のマーカーへ移動
35. `M / S / Volume / Pan`でトラックを調整
36. `Loop Clip`で選択中AudioClip / MidiClipの範囲を繰り返し再生
37. `Loop`でプロジェクト長の範囲を繰り返し再生
38. `Panic`または`Esc`で鳴りっぱなしのMIDIノートを停止
39. `Export Mix`でミックス結果を書き出す
40. `Export Track`で選択トラックだけをStemとして書き出す

## プロジェクト保存

`.aidaw`保存時、AudioClipが参照している音声素材はプロジェクトファイルと同じ階層の`Audio/`フォルダへコピーされます。
プロジェクトJSON内には`Audio/filename.wav`のような相対パスを保存するため、プロジェクトフォルダ単位で移動しやすくなっています。

## ショートカット

- `Space`: 再生 / 一時停止
- `Home`: 再生位置を先頭へ移動
- `End`: 再生位置をプロジェクト末尾へ移動
- `Esc`: Panic / All Notes Off
- `Command + N`: 新規プロジェクト
- `Command + O`: プロジェクトを開く
- `Command + S`: プロジェクトを保存
- `Command + Shift + S`: 名前を付けてプロジェクトを保存
- `Command + Option + E`: ミックスを書き出し
- `Command + Option + Shift + E`: 選択トラックをStemとして書き出し
- `Command + Z`: Undo
- `Command + Shift + Z`: Redo
- `Command + Shift + A`: 全トラックのRecord Armを解除
- `Command + Shift + U`: 全トラックのMute / Soloを解除
- `Command + Option + D`: 選択中トラックを複製
- `Command + ← / →`: 選択中AudioClip / MidiClipを現在のスナップグリッド単位で左右へ移動
- `Command + Shift + ← / →`: 選択中AudioClip / MidiClipの右端を現在のスナップグリッド単位で伸縮
- `Command + Option + ← / →`: 再生位置を選択中AudioClip / MidiClipの先頭 / 末尾へ移動
- `Command + J`: 選択中AudioClip / MidiClipを再生位置へ移動
- `Tab` / `Shift + Tab`: 次 / 前のAudioClip / MidiClipを選択
- `Option + C`: 現在の再生位置に重なっているAudioClip / MidiClipを選択
- `Option + Q`: 選択中AudioClip / MidiClipの開始位置を最寄りの小節頭へ揃える
- `Option + ← / →`: 再生位置を前後の拍へ移動
- `Option + Shift + ← / →`: 再生位置を前後の小節へ移動
- `Option + ↑ / ↓`: 前後のトラックを選択
- `Option + Shift + ↑ / ↓`: 選択中トラックを同種トラック内で上下へ移動
- `Option + R / M / S`: 選択中トラックのRecord Arm / Mute / Soloを切替
- `Option + Shift + R`: 選択中トラックだけをRecord Armにする
- `Option + Shift + S`: 選択中トラックだけをSoloにする
- `Option + 0`: 選択中トラックのVolume / Panを基準値へ戻す
- `Option + Shift + 0`: Master Volumeを基準値へ戻す
- `Option + F`: プロジェクト全体が見えるようにTimeline Zoomを調整
- `Option + = / -`: メトロノーム音量を上下
- `Option + [ / ]`: スナップグリッドを4分、8分、16分、32分で切り替え
- `Option + M`: 現在位置にマーカーを追加
- `Option + Shift + M`: 再生位置付近のマーカー名を変更
- `Option + Delete` / `Option + Backspace`: 再生位置付近のマーカーを削除
- `Option + , / .`: 前後のマーカーへ移動
- `Option + B`: 現在の小節をループ範囲に設定
- `Option + Shift + B`: ループ範囲を解除
- `Command + D`: 選択中AudioClip / MidiClipを複製
- `Command + Shift + D`: 選択中AudioClip / MidiClipを再生位置へ複製
- `Command + Option + Shift + D`: 選択中AudioClip / MidiClipを次の小節頭へ複製
- `Command + Shift + M`: 選択中AudioClip / MidiClipのMute切替
- `Command + E`: 選択中AudioClip / MidiClipを再生位置でSplit
- `Command + Option + F`: 選択中AudioClipのFade In / Outをクリア
- `Command + Option + I / O`: 選択中AudioClipのFade In / Outを0.25秒増やす
- `Command + Option + Shift + I / O`: 選択中AudioClipのFade In / Outを0.25秒減らす
- `Command + Option + B`: 選択中AudioClipの両端に50msフェードを設定
- `Command + = / -`: 選択中AudioClipのClip Gainを上下
- `Command + 0`: 選択中AudioClipのClip Gainを1.0へ戻す
- `Command + Shift + N`: 選択中AudioClipのClip Gainをピーク基準でNormalize
- `Command + Shift + R`: 選択中AudioClipのReverse切替
- `Command + Option + R`: 選択中MidiClipを時間反転
- `Command + K`: 選択中MidiClipを現在のスナップグリッドでクオンタイズ
- `Command + Option + K`: 選択中MidiClipを現在のスナップグリッドでSwing Quantize
- `Command + Shift + G`: 選択中MidiClipを推定ルートのメジャースケールへ補正
- `Command + Option + G`: 選択中MidiClipを推定ルートのマイナースケールへ補正
- `Command + ↑ / ↓`: 選択中MidiClipを1オクターブ上下に移調
- `Command + Shift + ↑ / ↓`: 選択中MidiClipを半音上下に移調
- `Command + Shift + I`: 選択中MidiClipのピッチを反転
- `Command + Option + ↑ / ↓`: 選択中MidiClipへ上下オクターブを重ねる
- `Command + ] / [`：選択中MidiClipのVelocityを上下
- `Command + Option + V`: 選択中MidiClipのVelocityを80%に統一
- `Command + Shift + H`: 選択中MidiClipのVelocityをHumanize
- `Command + Option + H`: 選択中MidiClipのタイミングをHumanize
- `Command + Shift + L`: 選択中MidiClipのノート長をLegato化
- `Command + Option + L`: 選択中MidiClipのノート長をStaccato化
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
- AudioClip編集はSplit、左右端トリム、複製、削除、1秒フェード、Clip Gainなどに対応
- MidiClip編集は移動、右端トリム、Split、複製、削除、グリッドクオンタイズ、移調、Velocity調整、Humanize、Legato / Staccatoに対応
- Audioタイムストレッチは未実装
- VST / AUプラグインには未対応
- MIDI CC編集は未実装
- AI Chords / AI Bass / AI Arpはローカルの簡易MIDI生成で、外部AI APIは未接続
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
