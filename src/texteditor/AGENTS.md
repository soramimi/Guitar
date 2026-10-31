# texteditor

## プロジェクトの目的

C++/Qt による、折り返し(ワードラップ)・Unicode・IME 入力に対応したテキストエディタの基盤を
開発中。まだ基礎部分(文字処理・折り返し行の管理・描画パイプライン)を実装している段階で、
検索/置換・Undo/Redo・シンタックスハイライトなど、実用エディタとしての機能はまだ実装されて
いない。

## 全体構成

```
MainWindow (QMainWindow, TextEditorApp/src)
 ├─ PtyProcessController → AbstractPtyProcess → POSIX PTY / Windows ConPTY
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
  (`AbstractTextEditorApplication::Font`)を担当。連続 resize は 30ms デバウンスしてから
  折り返し幅を更新する
- **TextEditorWidget.h/.cpp** — View と横/縦スクロールバーをまとめる薄いコンテナ
- **TextEditorTheme.h/.cpp** — Light/Dark の配色定義
- **InputMethodPopup.h/.cpp** — IME 変換候補ポップアップ(プラットフォームによっては無効化)
- **unicode.h/.cpp** — UTF-8/16/32 の相互変換ライブラリ(Qt 非依存)
- **UnicodeWidth.h/.cpp** — 文字の表示幅判定(East Asian Width)
- **WrapAlgorithm.h/.cpp** — コードポイント列と幅計測コールバックから折り返し終端を求める
  Qt 非依存アルゴリズム。`WrapAlgorithm/` に GoogleTest を持つ
- **EscapeSequence.h/.cpp** — ターミナルモード用ANSIエスケープシーケンス解析(実験機能)
- **TerminalScreen.h/.cpp** — Qt非依存の端末ストリーム解析・固定セル画面モデル。UTF-8/C0/CSI/SGRを
  チャンク継続状態で解析し、`CSI J/K`による画面・行消去、標準DA/DSR/カーソル位置照会への
  返信、スクロールバック、端末カーソル、全角・結合文字、前景・背景色を保持する。返信は
  `take_pending_responses()`で取り出し、画面テキストとして投影しない。`?47`/`?1047`/`?1049`
  は追従抑止用のモードとしてだけ追跡し、独立した代替画面バッファはP3で実装する
- **TextEditorApp/src/PtyProcessController.h/.cpp** — `process/`のPTY実装をQtイベントループへ接続し、
  プラットフォーム別生成、入力キュー、セッションID/状態、10msごとの逐次出力、終了通知、端末セルサイズ変更を担当する。
  PTY書込み・停止・resizeはGUIスレッドから実行しない。入力待ちキューは1 MiBで上限を設ける
- **TextEditorApp/src/PtyProcessFactory.h/.cpp** — `createPtyProcess(PtyProcessBackend)`でPTYを生成する。
  Windowsは`Direct`で`ProcessWinConPty`、`Worker`で`ProcessWinConPtyWithWorker`を選択する。
  Controllerは既定で`Worker`を使用し、`setBackend()`で次回起動時の方式を変更できる。
  POSIXではどちらも`ProcessPosixPty`を使用する。Windowsのアプリビルドは対応する
  `conpty-worker.exe`もアプリと同じ出力先へ生成する
- **PatternState.h/.cpp** — キーワード一致検出(実験機能、既定では無効)
- **TextEditorCoreTest/** — 幅LRU、タブ幅による失効、前景・背景の文字色属性、ANSI色状態に加え、
  Qt 6.3以前向けタブ幅計測、PTY Controllerの起動時入力・入力未読時のGUI応答・argv境界・端末返信の往復、
  およびViewの端末/通常入力分離を確認するQt依存のGoogleTest
- **PtyProcessTest/** — POSIX PTYのTTY接続、初期サイズ、ANSI保持、終了時drain、resize/SIGWINCH、
  起動直後にキューした入力の到達を確認し、通常neofetch出力をTerminalScreenへ適用するQt非依存のGoogleTest
- **TerminalScreenTest/** — カーソル移動、C0制御、チャンク分割、Unicodeセル、SGR、スクロール、
  リサイズ、標準端末照会への返信を確認するQt非依存のGoogleTest
- **LineIndexMap/** — 折り返し行の論理行⇔表示行マッピング用 B+-tree(独立ライブラリ、
  GoogleTest によるテスト完備)。詳細は [LineIndexMap/AGENTS.md](LineIndexMap/AGENTS.md) を参照
- **TextEditorApp/** — 上記部品を組み込んだ実行可能アプリ(サンプル/動作確認用)
  - `src/MainWindow.*` — メインウィンドウ、メニュー(File→Open/Save/Test)
  - Debugアクションは`on_action_debug_triggered()`先頭の`program`と`arguments`で定義した
    `bash --noprofile --norc -i`を`process_controller`経由でPTY上に起動し、入出力を端末Viewへ渡す
  - `writeToTerminal()`は、通常画面を表示中で出力前から末尾を表示しており、かつ出力が画面を
    実際にスクロールした場合だけ末尾へ追従する。代替画面モード中と履歴を閲覧中は追従しない
  - `src/MySettings.*` — `QSettings` ラッパーによる設定の永続化(INI)
  - `src/main.cpp` — エントリポイント

## 編集・描画のデータフロー

1. 通常エディタのキー入力 → `TextEditorView::keyPressEvent()` → `AbstractTextEditorApplication::write(QKeyEvent*)`
2. 文字の挿入/削除 → `Document::Line` を変更
3. `commit_line()` → `_update_line()` → `wrap_line()` → `LineIndexMap` を更新
4. `TextEditorView::paintEvent()` → `property_of_visual_line()` → `parse_line()` で解析済み文字列を取得
   → `QPainter` で描画(テキスト・選択範囲・カーソル・行番号)

端末モードではPTY出力を`TerminalParser`→`TerminalScreen`へ渡し、変更後の固定セル画面を
自己完結したANSI論理行としてDocumentへ投影する。`TextEditorView`は端末フォーカス時に
`terminalInputRequested(QByteArray)`だけを発行し、`MainWindow`が`PtyProcessController::writeInput()`へ
接続する。端末入力でDocumentを直接編集・ローカルエコーしない。P0では通常文字、IME確定、Enter、
Backspace、Escape、Ctrl+A〜Zを送信し、Tab/Shift+Tabはフォーカス移動に使う。特殊キーのVT入力符号化はP1で拡張する。
端末カーソルはエディタの選択カーソルとは独立し、`TextEditorView`がセル座標から直接描画する。端末モードは常に`NoWrap`を使用する。

## 折り返しアルゴリズム

- `_wrap_line()` は論理行をコードポイント列へ変換し、Qt 非依存の
  `wrap_algorithm::compute_wrap_points()` に幅計測コールバックを渡す。戻された終端位置から
  `Document::Line` の折り返し断片を組み立てる
- 候補探索は現在位置から1、2、4、8…個先を測る指数探索で幅超過区間を見つけ、その区間だけを
  二分探索する。表示行ごとに残り全体の中央を測らないことが長い行での二乗化を防ぐ
- 論理行は `Utf16Text` により一度だけQStringへ変換し、コードポイント位置からUTF-16位置への
  オフセット表を作る。幅計測時はこの表から `mid()` の範囲を決め、サロゲートペアを分断しない
- CharWrap は全文字境界を分割候補とする
- WordWrap は各表示行の先頭でCharWrap状態から開始する。最初の分割可能位置に到達した後で
  WordWrapへ切り替え、表示行末までCharWrapへ戻さない。次の表示行では再びCharWrapから開始する
- 表示行の先頭にある長い単語はCharWrapとして文字単位に分割する。このため、正の幅で表示幅を
  超える断片が生じるのは、原則として1コードポイント自体が表示幅を超える場合だけとなる
- 終端位置は必ず単調増加し、空でない論理行の最後の終端はコードポイント数と一致する。
  幅が0以下、または最初の候補だけで幅を超える場合も、最低1候補進めて無限ループを防ぐ
- 幅計測結果は、同じ開始位置に対して終端が進むほど単調非減少であることを二分探索の前提とする

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
- `Font::text_width()` の文字列幅キャッシュは最大4,096件のLRU。64 UTF-16コード単位を超える
  キーは登録せず、フォントまたはタブ幅が変わったときに全消去する。並列折り返しでは
  スレッドごとに独立した `Font` とLRUを使用する。幅計測はQt 6.3以降の
  `QFontMetrics::horizontalAdvance(QString, QTextOption)`を使わず、`myHorizontalAdvance()`が
  タブ区切りの各runを従来版`horizontalAdvance(QString)`で測り、指定ピクセル幅の次のタブ停止位置へ進める
- `CharAttr::ForegroundColorSpecified` が立っていない文字はテーマの既定前景色で描画する。
  色値0は明示的な黒にもなるため、色値そのものを「指定なし」の番兵として使わない
- `CharAttr::BackgroundColorSpecified` が立っている文字は、行背景より上、選択背景より下に
  背景色を描画する。標準色・明色・256色・truecolorのANSI SGRに対応する
- `Document::Line` と `CharBuffer` は `shared_ptr` を使う浅いコピー。ただし `setDocument()` は
  呼び出し元への編集波及と並列レイアウト時の競合を防ぐため、テキストと表示属性を複製し、
  解析・折り返しキャッシュを引き継がない
- 全行再折り返しは既定8スレッドで、`setLayoutThreadCount()` により変更できる。実際に生成する
  スレッド数は論理行数以下とする。1スレッドと8スレッドの断片・LineIndexMap一致は
  `TextEditorCoreTest` で検証する

## ビルド

qmake プロジェクト。アプリ本体とQt依存の`TextEditorCoreTest`はC++20、
Qt非依存の`LineIndexMap`・`WrapAlgorithm`・`PtyProcessTest`・`TerminalScreenTest`はC++17。

- `TextEditorApp/TextEditorApp.pro` — アプリ本体。`QT += core gui widgets`
  (Qt6 系は `core5compat` も追加)。ビルド成果物は `TextEditorApp/_bin/`
- `LineIndexMap/LineIndexMap.pro` — Qt 非依存のヘッダオンリーライブラリ + GoogleTest
- `WrapAlgorithm/WrapAlgorithm.pro` — Qt 非依存の折り返しアルゴリズム + GoogleTest
- `TextEditorCoreTest/TextEditorCoreTest.pro` — Qt依存のキャッシュ・属性テスト + GoogleTest
- `PtyProcessTest/PtyProcessTest.pro` — `process/ptyprocess.pri`を使うPTY統合テスト + GoogleTest
- `TerminalScreenTest/TerminalScreenTest.pro` — Qt非依存の端末画面モデルテスト + GoogleTest

```
(cd TextEditorApp/build/Qt_6_9_0_Debug && qmake6 ../../TextEditorApp.pro && make)

(cd WrapAlgorithm && qmake6 WrapAlgorithm.pro && make && ./WrapAlgorithm)
# 期待出力: [  PASSED  ] 7 tests.

(cd TextEditorCoreTest && qmake6 TextEditorCoreTest.pro && make && ./TextEditorCoreTest)
# 期待出力: [  PASSED  ] 24 tests.

(cd PtyProcessTest && qmake6 PtyProcessTest.pro && make && ./PtyProcessTest)
# Linuxでneofetchがある場合の期待出力: [  PASSED  ] 4 tests.

(cd TerminalScreenTest && qmake6 TerminalScreenTest.pro && make && ./TerminalScreenTest)
# 期待出力: [  PASSED  ] 16 tests.

# 並列折り返しのThreadSanitizer検証
(cd TextEditorCoreTest && make distclean && \
 qmake6 TextEditorCoreTest.pro 'CONFIG+=thread_sanitizer' && make && \
 ./TextEditorCoreTest --gtest_filter=Layout.SingleAndEightThreadsProduceIdenticalResults)
```

## 既知の未実装・不完全な箇所

- Undo/Redo、検索・置換、シンタックスハイライト、複数ドキュメント(タブ)は未実装
- diff 表示用の `CharFlags`/`TextEditorTheme` の配色は定義済みだが、diff 解析処理自体は未実装
- `MainWindow::on_action_file_save_triggered()` は保存先が `/tmp/test.txt` に固定(暫定実装)
- ターミナル画面のリサイズは既存セルの切り詰め・空白拡張であり、既存内容のリフローは行わない
- 折り返し断片は現在、元論理行の範囲参照ではなく独立した UTF-8 文字列として生成するため、
  全幅変更時には断片文字列の再生成コストが残る
- 行ごとの `LineProperty` キャッシュには全体のメモリ上限がない。非常に大きなファイルで
  メモリ使用量が問題になる場合は、安定した行IDをキーとするLRUなどを検討する
- `openFile()` は解析・折り返しキャッシュによるメモリ増幅を抑えるため、入力を 64 MiB 以下に
  制限している。大容量ファイル対応にはストリーミングまたは遅延読み込みの別設計が必要
- 折り返し終端の選択には常設の単体・ランダムテストがあるが、Document編集・座標変換・描画を
  統合したエディタ本体の自動テストはまだない。編集結果を「毎回全再計算する参照実装」と
  比較するランダムテストの追加が望ましい
- P0のPTY入力経路はLinuxで自動テスト済み。Windows Direct/Workerの実機受入試験はP5で残している

## 規約・注意点

- インデントはタブ(既存コードに合わせる)
- `unicode.*`、`UnicodeWidth.*`、`WrapAlgorithm.*`、`LineIndexMap` は Qt 非依存として維持すること。
  `AbstractTextEditorApplication` は現在 Qt 型に依存しているため、Qt 非依存化する場合は
  入力イベント・色・フォント計測・クリップボード/ファイルI/Oの境界を先に設計する
- 行管理を変更するときは `logical_lines`、各行の `visual_lines`、`LineIndexMap` の三者を
  同じ更新処理内で同期させる。表示行番号だけを安定したキャッシュキーとして使用しない
- テキスト・フォント・タブ幅など、文字位置へ影響する値を追加するときは、対応する revision の
  更新と `full_wrap_update_needed` の設定を忘れないこと
- `Font` の幅計測条件を変えるときはLRUを失効させる。LRUのmapと使用順listは必ず同じ処理で
  更新し、長いキーを無制限に登録しない
- `CharAttr::color == 0`と`background_color == 0`は明示的な黒として有効なので、色指定の有無は
  必ず`ForegroundColorSpecified` / `BackgroundColorSpecified`で判定する
- 折り返し規則を変更するときは `WrapAlgorithm/` の固定テストと貪欲法によるランダム比較を更新し、
  空文字列・幅0・長い単語でも終端位置が必ず前進することを確認する
- 並列レイアウトを変更するときは1スレッドと8スレッドの統合テストに加え、
  `thread_sanitizer` CONFIGでデータ競合がないことを確認する
- `LineIndexMap` は独立ライブラリとして扱い、変更する際は
  [LineIndexMap/AGENTS.md](LineIndexMap/AGENTS.md) の不変条件・テスト方針も確認する
- PTY経路を変更するときは、隣接する`../process/AGENTS.md`にも従い、`PtyProcessTest`でANSIの非改変、
  終了時drain、初期サイズとresizeを確認する。完了コールバックからWidgetを直接操作しない
