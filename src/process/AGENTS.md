# ProcessExample - Agent Notes

このドキュメントは、ProcessExample の現在の実装を、作業するエージェント向けにまとめたものです。
人間向けの概覧は `README.md`、Windows ConPTY 実験の経緯は `ConPTY.md` を参照してください。

## 現在の方向性

- もともとは Windows ConPTY (`CreatePseudoConsole`) を使って Git などの対話プロセスを制御する実験コードだった。
- 現在は、アプリ組み込み用のマルチプラットフォーム汎用プロセス起動ライブラリへ作り替えている途中。
- ライブラリ本体は `src/`、実験/サンプルアプリは `sampleapp/` に分離済み。
- Windows では通常パイプ、ConPTY、winpty、ConPTY worker 分離構成を持つ。
- POSIX (Linux/macOS) では通常パイプと PTY (`posix_openpt`) の実装を持つ。
- `sampleapp/main.cpp` は実験/動作確認用のエントリポイント。ライブラリ本体は `src/AbstractProcess.h` と各 `BasicProcess*` / `Process*` クラスが中心。

## ビルド

qmake プロジェクトは 2 つ。ライブラリのソース一覧は共通フラグメント `process.pri` にまとまっており、両プロジェクトから `include()` される (`PROCESS_SRC` / `PROCESS_PRI` 変数を使う)。

Windows / MSVC:

```powershell
qmake process-example.pro
nmake

qmake conpty-worker.pro
nmake
```

Linux / macOS:

```sh
qmake process-example.pro
make
```

`process.pri` は `win32` スコープで Windows 専用ファイルを振り分けているため、POSIX 側では `BasicProcessPosix.cpp` のみがライブラリ実装としてビルドされる。Windows 専用ソースは `<windows.h>` を直接 include しており、Linux ではそもそもコンパイル対象にしない構成。

`ProcessWinConPtyWithWorker` を使う Windows 構成では、`process-example.exe` と同じ `_bin` ディレクトリに `conpty-worker.exe` が必要。worker は `conpty-worker.pro` で `CONPTY_WORKER` を定義してビルドする。

## プロジェクト構造

```text
process-example.pro              - 実験/サンプル実行用 qmake プロジェクト
conpty-worker.pro                - Windows ConPTY worker 用 qmake プロジェクト
process.pri                      - ライブラリのソース/ヘッダ一覧 (両 .pro から include される)

src/                             - ライブラリ本体
  AbstractProcess.h/cpp          - 抽象インターフェース。AbstractProcess / AbstractPtyProcess
  ProcessHelper.h/cpp            - process::helper::dir_string_t と PushDir (RAII カレントディレクトリ変更)
  ProcessWinHelper.h             - Windows 用ヘッダのみヘルパー (匿名名前空間)。
                                   IS_VALID_HANDLE / write_all / convert_str_to_wstr / convert_wstr_to_str /
                                   getProgram / AutoHandle / AutoProcessInformation / AutoPseudoConsole / VtStripper
  BasicProcessWin.h/cpp          - _AbstractBasicProcess と通常パイプ実装 BasicProcessWin
  BasicProcessWinConPty.h/cpp    - ConPTY 直接所有の BasicProcessWinConPty
  ProcessWin.h/cpp               - AbstractProcess 適合の ProcessWin (通常パイプ、stdout/stderr 別キュー)
  ProcessWinConPty.h/cpp         - AbstractPtyProcess 適合の ProcessWinConPty (ConPTY ラッパー)
  ProcessWinPty.h/cpp            - winpty ラッパー ProcessWinPty (互換用)
  ProcessWinConPtyWithWorker.h/cpp - ConPTY を別 worker exe に分離する実装
  BasicProcessPosix.h/cpp        - POSIX 実装。ProcessPosix / ProcessPosixPty

sampleapp/                       - 実験/サンプルアプリ側
  main.cpp                       - 実験用エントリポイント。Windows/POSIX と worker mode を分岐
  base64.h                       - ConPTY worker へのコマンド受け渡し用 Base64 (厳格な decode_checked あり)
  misc.h/cpp                     - Windows OpenSSH 探索 (find_windows_openssh)
  unicode_conversion.h/cpp       - UTF-8/UTF-16 変換ユーティリティ

winpty/                          - バンドルされた winpty ライブラリ
ConPTY.md                        - ConPTY 実験の躓きポイントと設計判断
README.md                        - プロジェクト概覧 (現行コードに追従させてある)
```

## レイヤ構成

### 抽象インターフェース (src/AbstractProcess.h)

旧 `AbstractProcess2.h` を改名・整理したものが現行の `AbstractProcess.h`。`AbstractProcess2.h` はもう存在しない。

- `AbstractProcess`: 通常の stdin/stdout/stderr パイプ型プロセス。純粋仮想のみ (`start(command, use_input)`, `wait`, `stop`, `is_running`, `get_exit_code`, `write_input`, `close_input`, `stdout_bytes`, `stderr_bytes`)。
- `AbstractPtyProcess`: 疑似端末型プロセス。共有状態 (`mutex_`, `cond_`, `change_dir_`, `completed_fn_`, `user_data_`, `output_queue_`, `output_vector_`, `stderr_bytes_`) を基底クラスが直接保持し、非 virtual の `write_output()` / `pop_output()` / `set_change_dir()` / `set_completion_callback()` / `notify_completed()` を提供する。
- 出力バッファ (output_queue_ / output_vector_) のロックは基底の `mutex_` に一元化されている。各実装は `write_output()` で追記し、`read_output()` は `pop_output()` に委譲する。独自 mutex でこれらを触ってはいけない。
- `set_max_output_queue_size()` で output_queue_ を上限付きにできる (0 = 無制限、超過時は古い方から破棄)。output_vector_ (結果バッファ) は常に無制限。
- `AbstractPtyProcess::get_message()` は deprecated。`clear_message()` あり。
- `set_completion_callback()` は `start()` 前に設定すること (実行中の変更はスレッドセーフではない)。
- `APP_GUITAR` / `QT_VERSION` 用の Qt 連携フックは削除済み (ヘッダに include の残骸があるのみ)。

確定した出力は基本的に `wait()` 後に `stdout_bytes()` / `stderr_bytes()` から読む。実行中の逐次出力は `read_output()` を使う。

### Windows 低レベル実装

`src/BasicProcessWin.h` に `_AbstractBasicProcess` があり、`ExecResult { started, exit_code, error_code, error_message }` を `wait()` の戻り値とする。両 Basic クラスは pimpl (`Private *m`) 構成で、終了コードを `last_exit_code` にキャッシュして `get_exit_code()` で返す。

共通の安定化機構 (両 Basic クラス):

- `terminate()` (純粋仮想): 子プロセスを `TerminateProcess` で強制終了する。`wait()` と並行に呼んでも安全なよう、起動後のプロセスハンドルをスナップショット保持し、`wait()` がハンドルを閉じる直前にクリアする (`snap_mutex` で保護)。デストラクタも `terminate()` → `wait()` するため、生存中の破棄でハングしない。
- `close_input()` / `write_input()` は `input_mutex` で直列化され、close と write のハンドル競合を防ぐ (BasicProcessWinConPty は入力転送スレッドも同じ mutex を取る)。
- `BasicProcessWin::wait_for_output()` は検索済み位置を保持するインクリメンタル検索 (呼び出しごとの全量文字列化による O(n^2) を回避)。

| クラス | ファイル | 方式 | 主な特徴 |
|---|---|---|---|
| `BasicProcessWin` | `src/BasicProcessWin.h/cpp` | 匿名パイプ + `CreateProcessW` | stdout/stderr を同じパイプに統合。`Options` で stdout 中継 / 結果ベクタ / 逐次キューを切替。`wait_for_output()` と完了コールバック (`set_completion_callback` / `notify_completed`) を持つ。 |
| `BasicProcessWinConPty` | `src/BasicProcessWinConPty.h/cpp` | `CreatePseudoConsole` + `CreateProcessW` | ConPTY を直接所有。入力転送スレッド、出力読み取りスレッド、状態保持型 `VtStripper` を持つ。`Options::vt_stripped` (既定 true)、`Options::no_window` / `set_no_window()`。 |

- `misc::get_error_message()` と `misc::build_command_line()` は `src/BasicProcessWin.h` で宣言、`src/BasicProcessWin.cpp` で定義されている (sampleapp の misc とは別物なので注意)。
- RAII ハンドル (`AutoHandle` / `AutoProcessInformation` / `AutoPseudoConsole`) と `VtStripper`、`write_all`、UTF-8/UTF-16 変換 (`convert_str_to_wstr` / `convert_wstr_to_str`)、`getProgram` は `src/ProcessWinHelper.h` にヘッダのみ実装 (匿名名前空間)。旧 `wstring.h/cpp` は削除済み。
- `BasicProcessWinConPty::is_conpty_available()` は `kernel32.dll` の `CreatePseudoConsole` を確認する。Windows 10 version 1809 以降が前提。

### Windows ラッパー

| クラス | ファイル | 基底 | ラップ対象 | 備考 |
|---|---|---|---|---|
| `ProcessWin` | `src/ProcessWin.h/cpp` | `AbstractProcess` | 内部の `ProcessWinThread` | 通常パイプ版。stdout/stderr を別キューで取得。環境変数ブロックをキャッシュし `LANG=en_US.UTF8` を追加 (既存の LANG は尊重)。`stop()` は `TerminateProcess` で強制終了してから wait する。起動失敗は `get_error_code()` / `get_error_message()` で取得可能。 |
| `ProcessWinConPty` | `src/ProcessWinConPty.h/cpp` | `AbstractPtyProcess` | `BasicProcessWinConPty` | ConPTY 直接所有版。`output_vector` 既定オン。stderr は空扱い。`set_no_window()` / `set_options()` を持つ。`wait()` は `conpty.wait()` でブロックしている間に state mutex を保持しないため、待機中の `write_input()` / `read_output()` / `stop()` が機能する。`stop()` は `terminate()` で子を終了させてから回収する。 |
| `ProcessWinPty` | `src/ProcessWinPty.h/cpp` | `AbstractPtyProcess` | winpty | 互換用。現在の主経路ではない。起動失敗の理由は `get_error_message()` に入る (exit code は一律 127)。 |
| `ProcessWinConPtyWithWorker` | `src/ProcessWinConPtyWithWorker.h/cpp` | `AbstractPtyProcess` | `BasicProcessWin` + `conpty-worker.exe` | ConPTY を別プロセスへ分離する構成。Qt Creator など外側の疑似端末との干渉回避が目的。`wait_for_output()` で worker 出力からプロンプト検出できる。`stop()` は worker を強制終了してから回収する。 |

### ConPTY worker 分離構成

`ProcessWinConPtyWithWorker` は、監督プロセスと worker プロセスの 2 段階構成。

1. 監督側は `start()` で ConPTY の利用可否 (`is_conpty_available()`) と、`GetModuleFileNameW(nullptr, ...)` で得た自 exe 隣の `conpty-worker.exe` の存在を確認してから、`conpty-worker.exe --conpty-worker-- <base64-command>` を `BasicProcessWin` で起動する。
2. worker 側は `ProcessWinConPtyWithWorker::run_worker(argc, argv)` で worker mode を判定する。worker mode の識別タグは `--conpty-worker--`。
3. worker オプションとして `--no-window` があり、`BasicProcessWinConPty::Options::no_window` に反映される。
4. コマンド文字列は Base64 で渡す。worker 側は `Base64::decode_checked()` で厳格に検証し、デコード失敗・空コマンド・NUL 混入を拒否する。**worker 側のエラー (引数不正・デコード失敗・子の起動失敗) は終了コード `126` を返す** (子プロセス自身の終了コードと区別するため。`128` は子が返しうる値なので使わない)。
5. worker 側は `BasicProcessWinConPty` (`output_stdout = true`) で実コマンドを起動し、worker の stdin/stdout が監督側のパイプにつながる。
6. 監督側は `wait_for_output()` でプロンプトを検出し、`write_input()` で worker 経由の ConPTY へ入力を送れる。
7. worker 側の ConPTY 終了は `BasicProcessWin` の完了コールバック経由で監督側に通知され、`notify_completed()` に中継される。監督側の `m->running` はこのコールバックが別スレッドから更新するため `std::atomic<bool>`。

なお `src/ProcessWinConPtyWithWorker.cpp` は `sampleapp/base64.h` と `sampleapp/misc.h` を include しており、ライブラリ側が sampleapp 側ヘッダに依存している (両 .pro の INCLUDEPATH で解決している)。

### POSIX 実装 (src/BasicProcessPosix.h/cpp)

| クラス | 基底 | 方式 | 主な特徴 |
|---|---|---|---|
| `ProcessPosix` | `AbstractProcess` | `pipe()` + `fork()` + `execvp()` | stdout/stderr を別パイプで取得。入力はキュー経由で子 stdin へ書き込む。子で `LANG=C` を設定。exec 失敗時は `_exit(127)`。 |
| `ProcessPosixPty` | `AbstractPtyProcess` | `posix_openpt()` + `fork()` + `execvp()` | PTY master/slave を使い、`select()` で master fd を監視。VT ストリッピングは行わない。 |

POSIX 側の安定化機構:

- **SIGPIPE 対策**: `ProcessPosixThread` のスレッドは開始時に `pthread_sigmask()` で SIGPIPE をブロックする。子が stdin を閉じた後の `write()` は EPIPE エラーとして処理され、ホストプロセスがシグナルで落ちることはない。
- **強制終了エスカレーション**: `stop()` / デストラクタは SIGTERM を送り、約 2 秒で終了しない子には SIGKILL へエスカレーションする (終了コードは `128 + signal`)。SIGTERM を無視する子でハングしない。
- **PTY 出力の drain**: `ProcessPosixPty` は子の終了を検出した後、master fd が EIO/EOF になるまで残りの出力を読み切ってから close する (最後の出力を取りこぼさない)。
- **fork 安全性**: argv 構築と env 文字列のコピーは fork 前に済ませ、fork 後の子では `execvp` と `write(2)` のみを使う (`fprintf` は使わない)。子の `setenv`/`putenv` は残っている (下記「既知の未整理点」参照)。
- pipe/fork 失敗時の `error_message` は `get_error_message()` で取得できる。

POSIX 側の注意:

- 子プロセスのシグナル終了コードは `128 + signal` に寄せている。
- `ProcessPosixPty::close_input()` は現状空実装。
- `ProcessPosixPty` は `env` 文字列を `putenv()` に渡す簡易実装で、環境ブロック全体の一般的な表現にはまだなっていない。
- コマンドライン分割は `ProcessPosix::parse_args()` (静的) と `ProcessPosixPty` 用の `make_argv()` の 2 系統が残っている。`"""` はリテラル `"` に展開する独自仕様で、空引数は捨てられる (ソース内コメント参照)。

## sampleapp/main.cpp の現状

`sampleapp/main.cpp` はライブラリ API の完成形ではなく、実装確認用のサンプル/実験コード。

Windows:

- `CONPTY_WORKER` 定義時は `ProcessWinConPtyWithWorker::run_worker(argc, argv)` だけを呼ぶ worker exe 用 main になる。
- 通常ビルドでは `git --version` を使い、6 方式を順に全実行する: `main_basic_win` (BasicProcessWin) → `main_basic_win_conpty` (BasicProcessWinConPty) → `main_win` (ProcessWin) → `main_win_conpty` (ProcessWinConPty) → `main_win_conpty_with_worker` (worker 分離) → `main_winpty` (winpty)。
- `main_win_conpty_with_worker()` は最初に `misc::find_windows_openssh()` を呼び、OpenSSH が見つからなければエラー終了する。現状のサンプルではその値をコマンドラインに差し込まず、引数で受けた `cmd` をそのまま worker 分離版で実行する。
- `wait_for_output("Are you sure you want to continue connecting")` を検出したら `write_input("no\n", 3)` を送るデモコードは残っているが、現在の `git --version` サンプル経路では通常発火しない。

POSIX:

- 固定 `select = 0` により `ProcessPosix` で `/usr/bin/git --version` を実行する。
- `select = 1` で `ProcessPosixPty` を使う。

## 重要な設計判断

- ConPTY では stdout/stderr が端末出力として統合されるため、ConPTY 系の `stderr_bytes()` は基本的に空。
- ConPTY 出力には VT シーケンスが混ざるため、`BasicProcessWinConPty` は `VtStripper` (ProcessWinHelper.h) で状態を保持しながら除去する。`ReadFile` のチャンク境界をまたぐシーケンスにも対応する。
- ConPTY の入力転送スレッドは `PeekNamedPipe()` で stdin 側のデータ有無を見てから `ReadFile()` する。終了時に `ReadFile()` で永久待ちになることを避けるため。
- `ProcessWinConPtyWithWorker` は、外側の疑似端末や IDE 組み込み端末と内側の ConPTY の寿命・ハンドル干渉を避けるため、ConPTY 所有プロセスを分離している。
- Git for Windows 同梱の MSYS ssh は、ConPTY 環境で確認用 TTY を再取得できず `Host key verification failed.` になる場合がある。ホスト鍵確認プロンプトの実験では Windows OpenSSH を明示的に使う。
- `BasicProcessWin` と `BasicProcessWinConPty` は `Options` で、標準出力への中継、結果バッファ、逐次キューを個別に有効化する。
- `stop()` / デストラクタは子プロセスを確実に終了させる方針。Windows 系は `TerminateProcess`、POSIX 系は SIGTERM → 約 2 秒 → SIGKILL。実行中の子を残したまま破棄・停止してハングすることを防ぐ。
- `terminate()` (Windows Basic クラス) はプロセスハンドルのスナップショットを専用 mutex で保護し、`wait()` 中のハンドルクローズとの競合 (閉じたハンドル値の再利用による誤 kill) を防ぐ。
- PTY 出力バッファのロックは `AbstractPtyProcess::mutex_` に一元化。`write_output()` / `pop_output()` 以外から output_queue_ / output_vector_ を直接触らない。

## 作業時の注意

- このリポジトリは実験からライブラリへ移行中で、古いコメントがソース内に残っている場合がある。挙動はソースコードを優先して確認する。
- `AbstractProcess2.h` ではなく、現行は `src/AbstractProcess.h` を使う。`main.h` は存在しない。`wstring.h/cpp` は削除済み。
- `process-example.pro` は実験用、`conpty-worker.pro` は worker exe 用。Windows で worker 分離構成を試すなら両方ビルドする。ライブラリのファイル追加/削除は `process.pri` に反映する。
- ConPTY の詳細な経緯や既知の躓きポイントは `ConPTY.md` に追記する。
- コード変更時は Windows/POSIX の両方に影響するヘッダ分岐を確認する。特に `_WIN32`、`APP_GUITAR`、`QT_VERSION`、`CONPTY_WORKER` の条件に注意する。
- Linux では `qmake process-example.pro && make` でビルドできることを確認済み (2026-07 時点)。Windows 専用ファイルの変更は Linux ではコンパイル検証できないため慎重に。

## 既知の未整理点

- `src/ProcessWinConPtyWithWorker.cpp` が `sampleapp/base64.h` / `sampleapp/misc.h` に依存しており、ライブラリが自己完結していない。
- `misc` 名前空間が分散している: `misc::get_error_message` / `misc::build_command_line` は `src/BasicProcessWin.*`、`find_windows_openssh` は `sampleapp/misc.*`。
- Windows 側には `BasicProcessWin` と `ProcessWinThread` (ProcessWin.cpp 内部) の 2 系統の通常パイプ実装がある。
- `ProcessWinPty` は残っているが、現在の主経路は ConPTY と ConPTY worker 分離構成。
- `ProcessPosixPty::close_input()` は空実装、`env` は `putenv()` ベースの簡易実装。
- `sampleapp/main.cpp` は実験用ハーネスのままで、ライブラリ利用時の正式なサンプルとしては整理途中。
- **孫プロセスによるパイプ保持**: 子が孫プロセスに stdout/stderr パイプを継承させると、子を kill しても孫がパイプを開き続けるため EOF が来ず、出力読み取りスレッドの join がブロックされる。現状は既知の制限 (Windows は `CancelSynchronousIo`、POSIX はタイムアウト付き reader などの対策が今後の課題)。
- POSIX の fork 後の子で `setenv`/`putenv` をまだ使っている (glibc では内部で malloc しうる)。マルチスレッドホストでの完全な fork 安全性には `posix_spawn` への置き換えが今後の課題。
- `ProcessWinThread::cached_env_` は初回起動時の環境ブロックをキャッシュし続ける。ホストが後から環境変数を変更しても子プロセスに反映されない (パフォーマンス優先の意図的な仕様)。
- `BasicProcessWinConPty` の入力転送スレッドは、ConPTY 入力パイプが満杯で子が読まない場合に `write_all` 内でブロックしうる (この間 `close_input()` 等も待たされる)。
