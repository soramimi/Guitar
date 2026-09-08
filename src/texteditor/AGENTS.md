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
        └─ AbstractTextEditorApplication (編集・折り返し・座標変換ロジックの基底クラス)
           └─ TextEditorEngine (shared_ptr) → Document → logical_lines
```

- **AbstractTextEditorApplication.h/.cpp** — エディタ本体のロジック(カーソル移動、選択、
  クリップボード、ファイルI/O、折り返し計算など)を持つ基底クラス。`Character` / `Document` /
  `TextEditorContext` などのデータ構造もここに定義。マルチスレッド(8スレッド)で
  `update_visual_lines_all()` により全行の折り返しを再計算する。基底クラスだが現在は
  `QKeyEvent` / `QColor` / `QFont` などの Qt 型に依存している
- **TextEditorView.h/.cpp** — `AbstractTextEditorApplication` を継承した `QWidget`。
  `paintEvent` での描画、マウス/キーボード/IME イベント処理、フォントメトリクスのキャッシュ
  (`AbstractTextEditorApplication::Font`)を担当。連続 resize は 75ms デバウンスしてから
  折り返し幅を更新する
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
4. `TextEditorView::paintEvent()` → `queryFormattedLine()` → `parseLine()` で解析済み文字列を取得
   → `QPainter` で描画(テキスト・選択範囲・カーソル・行番号)

## 行管理とレイアウトキャッシュ

- `Document::logical_lines` が文書の唯一の正本。各 `Document::Line` の
  `Meta::visual_lines` は、その論理行を現在の幅で分割した表示行(折り返し断片)
- 表示行の平坦な配列は持たない。表示行番号 `vrow` は `LineIndexMap::visual_to_logical()` で
  `(logical row, wrap index)` に変換し、対象論理行の `visual_lines[wrap_index]` を直接参照する
- `LineIndexMap` は論理行ごとの折り返し数と各断片のコードポイント数を保持する。
  `logical_lines` の挿入・削除時は、必ず同じ位置の `LineIndexMap` エントリも更新する
- UTF-8 デコードと文字の X 座標計算結果は、表示行番号をキーにしたLRUではなく、
  各 `Document::Line::Meta::detail` (`LineProperty`) に保存する。`text_revision` と
  `metrics_revision` が一致する場合のみ再利用する
- 通常の文字編集では変更された論理行だけを再解析・再折り返しする。後続論理行は再計算しない
- 幅変更では全論理行の折り返し境界を更新するが、フォントが同じならデコード・文字幅計測結果を
  再利用する。同じ幅での再レイアウト要求は `full_wrap_update_needed` によりスキップする
- `Document::Line` と `CharBuffer` は `shared_ptr` を使う浅いコピー。ただし `setDocument()` は
  呼び出し元への編集波及と並列レイアウト時の競合を防ぐため、テキストと表示属性を複製し、
  解析・折り返しキャッシュを引き継がない

## ビルド

qmake プロジェクト(Qt Creator, C++17)。

- `TextEditorApp/TextEditorApp.pro` — アプリ本体。`QT += core gui widgets`
  (Qt6 系は `core5compat` も追加)。ビルド成果物は `TextEditorApp/_bin/`
- `LineIndexMap/LineIndexMap.pro` — Qt 非依存のヘッダオンリーライブラリ + GoogleTest

```
cd TextEditorApp/build/Qt_6_9_0_Debug && qmake6 ../../TextEditorApp.pro && make
```

## 既知の未実装・不完全な箇所

- Undo/Redo、検索・置換、シンタックスハイライト、複数ドキュメント(タブ)は未実装
- diff 表示用の `CharFlags`/`TextEditorTheme` の配色は定義済みだが、diff 解析処理自体は未実装
- `MainWindow::on_action_file_save_triggered()` は保存先が `/tmp/test.txt` に固定(暫定実装)
- 折り返し断片は現在、元論理行の範囲参照ではなく独立した UTF-8 文字列として生成するため、
  全幅変更時には断片文字列の再生成コストが残る
- 行ごとの `LineProperty` キャッシュには全体のメモリ上限がない。非常に大きなファイルで
  メモリ使用量が問題になる場合は、安定した行IDをキーとするLRUなどを検討する
- `openFile()` は解析・折り返しキャッシュによるメモリ増幅を抑えるため、入力を 64 MiB 以下に
  制限している。大容量ファイル対応にはストリーミングまたは遅延読み込みの別設計が必要
- エディタ本体には LineIndexMap のような常設の自動テストがまだない。折り返し・編集・座標変換を
  「毎回全再計算する参照実装」と比較するランダムテストの追加が望ましい

## 規約・注意点

- インデントはタブ(既存コードに合わせる)
- `unicode.*`、`UnicodeWidth.*`、`LineIndexMap` は Qt 非依存として維持すること。
  `AbstractTextEditorApplication` は現在 Qt 型に依存しているため、Qt 非依存化する場合は
  入力イベント・色・フォント計測・クリップボード/ファイルI/Oの境界を先に設計する
- 行管理を変更するときは `logical_lines`、各行の `visual_lines`、`LineIndexMap` の三者を
  同じ更新処理内で同期させる。表示行番号だけを安定したキャッシュキーとして使用しない
- テキスト・フォント・タブ幅など、文字位置へ影響する値を追加するときは、対応する revision の
  更新と `full_wrap_update_needed` の設定を忘れないこと
- `LineIndexMap` は独立ライブラリとして扱い、変更する際は
  [LineIndexMap/AGENTS.md](LineIndexMap/AGENTS.md) の不変条件・テスト方針も確認する
