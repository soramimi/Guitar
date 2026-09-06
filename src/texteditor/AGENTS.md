# texteditor

## プロジェクトの目的

C++/Qt による、折り返し(ワードラップ)・Unicode・IME 入力に対応したテキストエディタの基盤を
開発中。まだ基礎部分(文字処理・折り返し行の管理・描画パイプライン)を実装している段階で、
検索/置換・Undo/Redo・シンタックスハイライトなど、実用エディタとしての機能はまだ実装されて
いない。

## 全体構成

```
MainWindow (QMainWindow, TextEditorApp/src)
 └─ TextEditorWidget (View + 縦横スクロールバーのコンテナ)
     └─ TextEditorView (QWidget: 描画・キーボード/マウス/IME入力)
        └─ AbstractTextEditorApplication (Qt非依存の編集ロジック基底クラス)
           └─ TextEditorEngine (shared_ptr) → Document → logical_lines
```

- **AbstractTextEditorApplication.h/.cpp** — エディタ本体のロジック(カーソル移動、選択、
  クリップボード、ファイルI/O、折り返し計算など)を持つ基底クラス。`Character` / `Document` /
  `TextEditorContext` などのデータ構造もここに定義。マルチスレッド(8スレッド)で
  `update_visual_lines_all()` により全行の折り返しを再計算する
- **TextEditorView.h/.cpp** — `AbstractTextEditorApplication` を継承した `QWidget`。
  `paintEvent` での描画、マウス/キーボード/IME イベント処理、フォントメトリクスのキャッシュ
  (`TextMetrics`)を担当
- **TextEditorWidget.h/.cpp** — View と横/縦スクロールバーをまとめる薄いコンテナ
- **TextEditorTheme.h/.cpp** — Light/Dark の配色定義
- **InputMethodPopup.h/.cpp** — IME 変換候補ポップアップ(プラットフォームによっては無効化)
- **unicode.h/.cpp** — UTF-8/16/32 の相互変換ライブラリ(Qt 非依存)
- **UnicodeWidth.h/.cpp** — 文字の表示幅判定(East Asian Width)
- **LineIndexMap/** — 折り返し行の論理行⇔表示行マッピング用 B+-tree(独立ライブラリ、
  GoogleTest によるテスト完備)。詳細は [LineIndexMap/AGENTS.md](LineIndexMap/AGENTS.md) を参照
- **TextEditorApp/** — 上記部品を組み込んだ実行可能アプリ(サンプル/動作確認用)
  - `src/MainWindow.*` — メインウィンドウ、メニュー(File→Open/Save/Test)
  - `src/MySettings.*` — `QSettings` ラッパーによる設定の永続化(INI)
  - `src/main.cpp` — エントリポイント

## 編集・描画のデータフロー

1. キー入力 → `TextEditorView::keyPressEvent()` → `AbstractTextEditorApplication::write(QKeyEvent*)`
2. 文字の挿入/削除 → `Document::Line` を変更
3. `commit_line()` → `_update_line()` → `wrap_line()` → `LineIndexMap` を更新
4. `TextEditorView::paintEvent()` → `queryFormattedLine()` → `parseLine()` +
   `calc_pos_x()` で座標計算 → `QPainter` で描画(テキスト・選択範囲・カーソル・行番号)

## ビルド

qmake プロジェクト(Qt Creator, C++17)。

- `TextEditorApp/TextEditorApp.pro` — アプリ本体。`QT += core gui widgets`
  (Qt6 系は `core5compat` も追加)。ビルド成果物は `TextEditorApp/_bin/`
- `LineIndexMap/LineIndexMap.pro` — Qt 非依存のヘッダオンリーライブラリ + GoogleTest

```
cd TextEditorApp/build/Qt_6_9_0_qt6_gcc_Debug && qmake6 ../../TextEditorApp.pro && make
```

## 既知の未実装・不完全な箇所

- `AbstractTextEditorApplication.cpp` の `_lines()` は `assert(0)` が残ったスタブ
  (NoWrap/Wrap で論理行・表示行を切り替える処理が未完成)
- 水平スクロールの計算(`update_horz_scroll()` 付近)が `if (0 && ...)` で無効化されたまま
- Undo/Redo、検索・置換、シンタックスハイライト、複数ドキュメント(タブ)は未実装
- diff 表示用の `CharFlags`/`TextEditorTheme` の配色は定義済みだが、diff 解析処理自体は未実装
- マウスドラッグによる範囲選択は未結線(クリックでのカーソル移動のみ)
- `MainWindow` の矢印キー用ハンドラ(`upArrow()` 等)は空のスタブ
- `MainWindow::on_action_file_save_triggered()` は保存先が `/tmp/test.txt` に固定(暫定実装)

## 規約・注意点

- インデントはタブ(既存コードに合わせる)
- Qt 依存部分(`TextEditorView` 以下)と Qt 非依存部分(`AbstractTextEditorApplication`,
  `unicode.*`, `UnicodeWidth.*`, `LineIndexMap`)を分離する設計を維持すること
- `LineIndexMap` は独立ライブラリとして扱い、変更する際は
  [LineIndexMap/AGENTS.md](LineIndexMap/AGENTS.md) の不変条件・テスト方針も確認する
