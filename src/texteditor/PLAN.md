# 実装計画と状況

未完了項目に加え、後続項目の前提となるP4の実装仕様を記録する。行番号は変わりやすいため、
場所は関数名で示す。

## 方針

- 実験機能は通常の編集・描画経路へ影響を広げず、対象を絞った自動テストを先に用意する
- 並列レイアウトへ変更を加える場合は、1/8スレッド一致テストとThreadSanitizerを確認する
- 文書・折り返し管理を変更する場合は、`logical_lines`、`visual_lines`、`LineIndexMap`を同じ更新処理内で同期する

## 優先順位

| 優先度 | 内容 | 状態 |
|---|---|---|
| P4 | ターミナルモード: PTYによるプロセス起動基盤 | 実装済み |
| P5 | ターミナルモード: ANSIカーソル移動と画面モデル | 計画済み |
| P6 | ターミナルモード: 行をまたぐSGR状態 | 設計待ち・P5経路では不要 |

---

## ターミナルモード（実験機能）

### PTYによるプロセス起動基盤

**状態: 実装済み。** LinuxではPTY単体テストとTextEditorAppのビルドを確認済み。
Windows ConPTY経路は実装済みだが、Windows実機でのビルド・実行確認は未実施。

旧実装の`MainWindow::on_action_debug_triggered()`は`QProcess`の通常パイプでneofetchを起動していた。
通常パイプでは子プロセスが端末ではないと判定し、色・進捗表示・改行処理などを変更する場合がある。
先にPTYの入出力・画面サイズ・終了処理を確定し、そのバイト列を次項の`TerminalParser`へ渡す。

リポジトリ内の`process/`にはPOSIXの`ProcessPosixPty`とWindowsの`ProcessWinConPty`が既にあるため、
PTYをTextEditorApp内で再実装せず、`AbstractPtyProcess`経由で再利用する。ただしGUI端末へ組み込むには、
画面サイズ、ANSI保持、出力drain、環境変数などの不足を先に補う。`process/`を変更するときは同ディレクトリの
`AGENTS.md`にも従う。

#### プラットフォーム

- Linux/macOS: `ProcessPosixPty`（`posix_openpt()`、`fork()`、`execvp()`）
- Windows 10 1809以降: `ProcessWinConPty`（`CreatePseudoConsole()`）
- PTYではstdoutとstderrを1本の端末ストリームとして扱う
- ConPTYが利用できないWindowsでは、起動失敗をターミナルへ表示する。通常パイプへ暗黙に戻して
  端末挙動を変えない

#### processライブラリの補強

`AbstractPtyProcess`へ、起動前と実行中の両方で使える端末サイズAPIを追加する。

```
resize_terminal(columns, rows)
```

POSIXではslaveの初期サイズとmasterへの`TIOCSWINSZ`を実装し、foreground process groupへ
`SIGWINCH`が届く状態にする。子プロセスは`setsid()`後にslaveをcontrolling terminalとして取得し、
GUIプロセスのstdin設定に依存しない通常のtermiosを設定する。`OPOST | ONLCR`を有効にし、PTY側で
LFからCRLFへの端末出力変換を行う。

Windowsでは`CreatePseudoConsole()`へ初期サイズを渡し、実行中は`ResizePseudoConsole()`を使う。
TextEditorAppではVTシーケンスを描画側へ渡す必要があるため、ConPTYの`vt_stripped`は必ずfalseにする。
GUIアプリでは親stdinを自動転送せず、将来の端末入力は`write_input()`から明示的に送る。

PTY出力は子の終了後もEOFまで読み、UIがキューを空にするまで`read_output()`できるようにする。
特にWindows版は`is_running() == false`になった後の残留キューも読み出せるよう修正する。端末表示では
結果全体の`output_vector`を重複保持せず、逐次キューだけを有効にできるオプションを追加する。

アプリ側の起動APIは次を個別に受け取り、既存processライブラリのコマンド文字列APIへの変換を
`PtyProcessController`内に閉じ込める。

- 実行ファイル
- 引数配列
- 初期PTYサイズ

これによりMainWindowへ独自のコマンドライン分割と引用符処理を漏らさない。子環境には少なくとも
`TERM=xterm-256color`と`COLORTERM=truecolor`を設定する。作業ディレクトリはprocessライブラリの
`set_change_dir()`で指定可能だが、現在のController APIではneofetchに不要なため公開しない。

#### TextEditorAppへの組み込み

既存の`process.pri`はworker版やwinptyまで含むため、TextEditorApp向けにはPOSIX PTYとConPTYだけを
含む最小の`ptyprocess.pri`を`process/`側に用意する。`TextEditorApp.pro`からこれをincludeし、
`MainWindow`に直接スレッド処理を置かず、QObjectの`PtyProcessController`を追加する。

```
MainWindow
  -> PtyProcessController
       -> AbstractPtyProcess
            -> ProcessPosixPty / ProcessWinConPty
```

`PtyProcessController`はプラットフォーム別インスタンス生成、起動・停止、出力drain、終了通知、
画面サイズ変更を担当する。PTYの完了コールバックはワーカースレッドから呼ばれるため、そこでWidgetを
操作せず、`QMetaObject::invokeMethod(..., Qt::QueuedConnection)`でGUIスレッドへ戻す。

実行中は10～16msのQTimerで`read_output()`を空になるまで読み、取得したバイト列を順序どおり
`writeToTerminal()`へ渡す。完了通知後にも最後のdrainを行い、その後にタイマー停止と`wait()`による
スレッド回収を行う。再実行時は前回のキューや完了状態を残さないようPTYオブジェクトを再生成する。

`TextEditorView`は端末セル数を返すAPIと、デバウンス後のサイズ変更通知を提供する。

```
columns = text area width / basis character width
rows    = view height / line height
```

値は最低1へクランプし、起動前に初期サイズを設定する。実行中のresizeはPTYへ通知する。

ウィンドウ破棄時は出力タイマーとコールバックを止め、子を停止し、最後の出力をdrainしてからスレッドと
PTYハンドルを回収する。POSIXではSIGTERM後に必要ならSIGKILL、Windowsでは`TerminateProcess()`を使う。

#### 実装順序

1. `process/`にPTYサイズ、VT保持、終了後drain、逐次出力専用オプションを追加する
2. POSIXのcontrolling terminal・termiosとWindows ConPTYの親stdin転送を整理する
3. PTYに必要なソースだけを公開する`ptyprocess.pri`を追加する
4. `PtyProcessController`とプラットフォーム別factoryをTextEditorAppへ追加する
5. `TextEditorView`の端末セル数・resize通知を接続する
6. `MainWindow`のQProcess経路をPTYへ置き換える
7. PTY単体テストとTextEditorAppのビルド確認を行う。neofetchの表示確認はGUI実機で行う

P5の画面モデルが完成するまでは、カーソル移動を必要としない
`neofetch --off --color_blocks off`を維持する。PTY化だけで通常のneofetchへ戻さない。

#### テスト

- 子のstdin/stdout/stderrで`isatty()`がtrueになる
- stdoutとstderrが順序を保った1ストリームとして届く
- ANSIシーケンスが除去・改変されない
- 子から取得した初期行列数がViewのセル数と一致する
- resize後に新しいサイズとPOSIXの`SIGWINCH`を確認できる
- 即時終了する子の最後の出力も欠落しない
- 完了後の残留出力をdrainしてからスレッドを回収する
- 停止時に直接の子プロセスとPTYハンドルを残さない。孫プロセスを含むプロセスツリー停止は
  `process/`側の既知課題として別途扱う
- 連続起動で前回の出力・終了状態が混ざらない
- 完了コールバックからGUIを直接操作しない
- neofetchの色とUTF-8出力が維持される

### ANSIカーソル移動と画面モデル

現在の`appendBulk()`は末尾追記専用で、`EscapeSequence`も論理行を描画用に遅延解析するときに
制御シーケンスを除去する。そのため、`CSI 19A`で過去の行へ戻り、`CSI 41C`で右へ移動して
既存内容を上書きするような端末出力は表現できない。通常の`neofetch`もASCIIロゴと情報欄を
横並びにするため、この種のカーソル移動を使用する。

#### 基本方針

- エディタの表示・選択カーソルとは別に、子プロセス出力専用の端末カーソルを持つ。ユーザーが
  過去の出力を選択またはスクロールしても、次の出力位置を変えない
- `Document`を直接端末画面として操作せず、Qt非依存の`TerminalParser`と`TerminalScreen`を追加する
- `TerminalScreen`を端末内容の正本とし、`Document`は描画・選択・コピー用の投影結果とする
- 端末カーソルの列はコードポイント番号ではなく表示セル番号として管理する
- 端末モードは`WrappingMode::NoWrap`を必須とする。エディタの折り返し表示行を端末行として扱わない
- PTYの1出力チャンクを解析し終えてから変更行をまとめて反映し、1文字ごとの再解析・再レイアウトを
  避ける

データフローは次のようにする。

```
PTYのバイト列
  -> TerminalParser（UTF-8・C0・CSI・SGRをチャンク継続状態で解析）
  -> TerminalScreen（固定セル画面・スクロールバック・端末カーソル・文字属性）
  -> Documentへの変更行投影
  -> TextEditorView
```

`TerminalParser`はUTF-8やCSIがPTYの読み取りチャンク境界で分割されても継続できる状態機械とする。
既存の`EscapeSequence`は行単位の遅延解析を前提とするため、ストリーム解析器としては流用しない。

#### 端末画面のデータ

各セルは最低限、コードポイント、文字属性、全角文字の継続セルかどうかを保持する。

```
TerminalCell
  - character
  - TerminalStyle
  - continuation

TerminalScreen
  - active screen rows
  - scrollback rows
  - cursor row / cell column
  - saved cursor
  - current style
  - cursor visibility
  - auto-wrap mode
```

ASCIIは幅1、全角文字は幅2、結合文字は幅0として扱う。幅2文字を上書きするときは、前半・後半の
両セルを消去して不正な継続セルを残さない。カーソル右移動先が行末より先なら、書き込み時に空白セルを
補う。

アクティブ画面の幅と高さはViewのセル数から与える。初期実装ではリサイズ時の文字再配置は行わず、
既存セルの切り詰めまたは空白拡張とする。リフローは別課題とする。

#### 第1段階で対応する命令

- C0: `LF`、`CR`、`BS`、`HT`
- CSI `A` / `B` / `C` / `D`: 上下左右移動
- CSI `E` / `F`: 次行・前行の行頭
- CSI `G`: 指定列
- CSI `H` / `f`: 指定行・列
- CSI `s` / `u`: カーソル位置の保存・復元
- CSI `?25h` / `?25l`: カーソル表示・非表示
- CSI `?7h` / `?7l`: 自動折り返しの有効・無効
- SGR: 現在対応しているリセット、前景色、背景色、256色、truecolor

移動命令の省略パラメーターと0は既定値1として扱い、巨大な移動量は画面端へクランプする。
未対応命令は文字として表示せず、シーケンス全体を安全に無視する。

P4でPTY slaveの`OPOST | ONLCR`を有効にするため、TerminalParserではLFとCRを本来の別命令として扱う。
PTYを経由しないテスト入力でも両者を暗黙に同一視しない。

#### Documentへの投影

変更された端末行だけを、行頭で属性を設定し行末でリセットする自己完結したANSI文字列へ直列化する。
これにより既存の`_parse_line()`、`CharAttr`、描画処理を再利用しつつ、各論理行を独立して解析できる。
端末画面からDocumentへ反映する専用APIを設け、同じ更新処理内で次を同期する。

- `Document::logical_lines`
- 各行の`visual_lines`
- `LineIndexMap`
- 解析・座標キャッシュのrevision

スクロールでアクティブ画面から押し出された行はスクロールバックへ移し、Document上では過去行として
保持する。アクティブ画面内のカーソル移動ではスクロールバック行を破壊しない。

この自己完結形式により、新しいTerminalScreen経路では行をまたぐSGR状態を必要としない。ただし、
従来の`appendBulk()`へ任意のANSIストリームを直接渡す経路には、後述のSGR継承問題が残る。

#### 実装順序

1. Qt非依存の`TerminalParser` / `TerminalScreen`と固定テストを追加する
2. C0制御、CSI A/B/C/D、空白補完、セル上書きを実装する
3. CSI E/F/G/H/f、保存・復元、カーソル表示、自動折り返しを追加する
4. 変更行をDocumentへ一括反映するアダプターと統合テストを追加する
5. `writeToTerminal()`を`TerminalScreen::feed()`経由へ変更する
6. `neofetch`から`--off`を外し、ASCIIロゴと情報欄の配置を確認する
7. 既に対応済みの背景色・256色描画を使い、`--color_blocks off`も外す

背景色と256色・truecolorは`CharAttr`、描画処理、SGR `38;5;n` / `48;5;n`、`38;2;r;g;b` /
`48;2;r;g;b`まで実装済み。TerminalScreenの`TerminalStyle`とDocument投影はこの色表現を再利用する。

#### テスト

- `abc ESC[2D XY`が`aXY`になる
- 複数行出力後に上移動・右移動し、既存行の指定セルから上書きできる
- CSIとUTF-8文字が複数の入力チャンクに分割されても結果が一致する
- 行末より右への移動後、書き込み位置まで空白セルが補われる
- 巨大な移動量が画面端へクランプされる
- CRによる同一行上書き、BS、タブストップ、LFを確認する
- 全角文字・結合文字・全角文字途中の上書きでセル不変条件が維持される
- SGR付き文字を上書きしたとき属性が新しい文字へ正しく適用される
- ユーザーの選択・スクロールが端末カーソル位置へ影響しない
- 出力反映後も`LineIndexMap::validate()`が成功する
- 通常のneofetch出力でCSI終端文字が本文へ漏れず、ASCIIロゴと情報欄が横並びになる

### SGR状態が次の論理行へ引き継がれない

各行は独立に解析・キャッシュ・並列レイアウトできる前提のため、ANSI SGRの色状態は次の論理行へ
引き継がれない。前景色・背景色のどちらにも同じ制限がある。対応する場合は、各行の行頭時点の
色状態を`Document::Line::Meta`などに保持し、
テキスト変更時に後続行の状態と解析キャッシュをどこまで更新するか設計する必要がある。

## 判断が必要な事項

1. 折り返し断片を独立したUTF-8文字列ではなく、元論理行への範囲参照へ変更するか

範囲参照化は全幅変更時の断片文字列再生成を避けられる一方、編集時の寿命管理、属性、描画キャッシュ、
`LineIndexMap`との同期方法を設計する必要がある。
