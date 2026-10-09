# AGENTS.md

## プロジェクト概要

このプロジェクトは、ローカルデスクトップGUIアプリケーション向けの **APIキー等の機密情報を安全に管理する仕組み** を構築するための実験・実装リポジトリです。

当初は単純な難読化（XOR + 固定シードの擬似乱数）のサンプルとして作成されましたが、現在は本格的な暗号化ベースのローカルVault機能へと移行・拡張を進めています。

## 目的

- APIキー等の機密情報を、PIN入力を条件に安全に暗号化/復号する
- OSが提供するセキュアストレージを優先的に利用する
- OSセキュアストレージが利用できない環境では、暗号化ファイルによるフォールバックを提供する
- 1Password API を使った外部キー保存も選択肢として残す
- アプリ本体はQtだが、APIキー管理機能（Vaultモジュール）は Qt 非依存とする

## 技術的制約

- GUIフレームワーク: Qt（継続）
- APIキー管理機能の Qt 依存: なし
  - `QByteArray`, `QFile`, `QSaveFile` 等は使用しない
  - バイナリ blob には `std::vector<char>`、識別キー・パスには `std::string` / `std::filesystem::path` を使用
  - `QtGui`, `QtWidgets` への依存は GUI レイヤーに限定
- 暗号化アルゴリズム: libsodium の XChaCha20-Poly1305（AEAD）
- 鍵導出: libsodium の Argon2id（opslimit 3、memlimit 256 MiB、MODERATE 相当）
- 乱数・安全な消去・メモリロック: libsodium
- 対象プラットフォーム: Linux, macOS, Windows（マルチプラットフォーム対応）

## 現状のコード

### 既存ファイル

| ファイル | 内容 |
|---|---|
| `src/main.cpp` | 自己テスト用エントリーポイント、およびGUIデモ |
| `src/vault/SecureBuffer.h/cpp` | libsodiumのガード付きメモリ（`sodium_malloc`）上の機密メモリコンテナ。再確保時も旧領域をゼロクリア。定数時間比較・ゼロクリア関数も提供 |
| `src/vault/SecretStorage.h` | セキュアストレージバックエンド I/F と `StorageStatus`（Phase 2 で追加） |
| `src/vault/ProcessHardening.h/cpp` | コアダンプ・ptrace アタッチ抑止（Phase 7 で追加） |
| `src/vault/Vault.h/cpp` | Argon2id / XChaCha20-Poly1305を用いる2段階鍵体系Vaultコア |
| `src/storage/FileBackend.h/cpp` | ファイルベースフォールバックバックエンド（Phase 2 で追加） |
| `src/storage/AtomicFile.h/cpp` | 0600作成・fsync・置換renameによるアトミック書き込み（Phase 7 で追加） |
| `src/storage/SystemKeychainBackend.h/cpp` | OSセキュアストレージバックエンド（Phase 3 で追加） |
| `src/gui/SecurePinEdit.h/cpp` | 入力をQStringに保持せずSecureBufferへ直接格納するPIN入力ウィジェット（Phase 7 で追加） |
| `src/gui/SetupVaultDialog.h/cpp/.ui` | Vault 新規セットアップダイアログ（Phase 9 で追加） |
| `src/gui/UnlockVaultDialog.h/cpp/.ui` | Vault 解除ダイアログ（Phase 9 で追加） |
| `src/gui/ResetVaultDialog.h/cpp/.ui` | Vault リセットダイアログ（Phase 9 で追加） |
| `src/gui/ChangePinDialog.h/cpp/.ui` | PIN 変更ダイアログ（Phase 9 で追加） |
| `src/gui/SecureStoreGUI.h/cpp` | 保存先選択・セットアップ・解除・リセットをまとめて行う GUI ヘルパー（Phase 9 で追加） |
| `qmake/localvault.pri` | 他のアプリから include するための qmake プロジェクト include ファイル（Phase 9 で追加） |
| `src/app/BackendSelector.h/cpp` | EMK保存先の選択・記録（Qt 非依存、Phase 7 で追加） |
| `qmake/localvault.pro` | Qt qmake プロジェクトファイル。`QT += core widgets` |

### 実装済みフェーズ

- **Phase 1**: 暗号化基盤（SecureBuffer）
- **Phase 2**: Vault コア + FileBackend
- **Phase 3**: SystemKeychainBackend（Linux libsecret / macOS Keychain / Windows DPAPI）
- **Phase 4**: PIN 入力ダイアログ + GUI デモ統合
- **Phase 5**: libsodiumを導入し、Argon2id / XChaCha20-Poly1305へ移行。EMK形式のバージョン化、`decryptToSecureBuffer()`、ロック済みバッファの再配置防止を追加
- **Phase 6**: Vault コア・ストレージバックエンドから Qt 依存を除去。`QByteArray` / `QString` を `std::vector<char>` / `std::string` / `std::filesystem::path` に置き換え
- **Phase 7**: 本番組み込みに向けた堅牢化
  - EMK の書き換えをステージングキー（`<emkKey>.pending`）経由・読み戻し検証付きにし、途中失敗で EMK を失わないようにした。バックエンドの `store()` は「削除→追加」をやめ置換保存に統一
  - `BackendSelector` で保存先を記録し、OSストレージの一時的な不調で FileBackend へ暗黙に切り替わらないようにした。FileBackend の新規使用はユーザー同意を必須とする
  - `VaultError` / `VaultState` / `StorageStatus` を導入し、PIN誤り・ストレージ不達・データ破損等を区別
  - `SecureBuffer` を `sodium_malloc` ベースのアロケータに変更、`SecurePinEdit` 導入、コアダンプ抑止
- **Phase 8**: 追加の堅牢化
  - libsecret の読み出しを `secret_password_search_sync`（`SECRET_SEARCH_ALL`）に変更し、ロック中の項目を「存在しない」と誤認しないようにした。`isAvailable()` は `secret_service_get_sync` による接続確認のみとし、解除ダイアログを出さない
  - `setup()` は本キーへの書き込み直前に EMK が存在しないことを再確認する（多重防御）
  - Windows で `QString` → `std::filesystem::path` を UTF-16 経由で変換（非 ASCII ユーザー名対策）
  - `Vault::encrypt(SecureBuffer const &, Blob &)` を追加
  - 全モジュールを `namespace localvault` に移動
  - GUI 層から libsodium の直接呼び出しを除去（`SecureBuffer::equals()` / `secure_memory::zero()` を使用）
- **Phase 9**: GUI の強化と再利用性の向上
  - `PinDialog` を廃止し、専用ダイアログ `SetupVaultDialog` / `UnlockVaultDialog` / `ResetVaultDialog` / `ChangePinDialog` を追加
  - 保存先選択から Vault 操作までをまとめて行う `SecureStoreGUI` を追加
  - `Vault` / `ISecretStorageBackend::load` / `AtomicFile::read` 等の出力引数をポインタに統一
  - `SystemKeychainBackend` にスキーマ名・サービス名の設定関数を追加
  - 空 PIN の許可・禁止をコンパイル時に選択可能にした
  - 他アプリからの組み込みを容易にする `qmake/localvault.pri` を追加

### 既知の課題・未決事項

- 旧難読化・独自暗号プリミティブは削除済み。新規の機密データ保存はVault APIのみを使用する

#### 未検証

- macOS / Windows のコードは Linux 上でのみ編集しており、実機ビルド・テストを行っていない
- libsecret で解除ダイアログを「キャンセル」した場合の挙動は実機で未確認。`SECRET_SEARCH_ALL` により項目が返り、`secret_item_get_locked()` が真になる（→ `Unavailable`）想定である。万一 `NotFound` 相当になっても `setup()` の書き込み直前の再確認で上書きは防がれるが、その時点でも見えなければ防げない
- libsecret のストアに対する保存時の解除ダイアログのキャンセルは `Error` として扱われ、`Unavailable` と区別できない
- GUI（`SecurePinEdit` の入力、各ダイアログ遷移）は手動確認が必要

#### 仕様として未決定（本番組み込み前に決めること）

- **保存先の移行**: FileBackend に同意して作成した Vault は、後で OS ストレージが使えるようになっても自動移行しない。現状は「必要なデータを復号・退避 → `--reset --yes` → 再セットアップ」で対応する。移行機能を提供するか決める
- **複数プロセス**: 同時起動したインスタンスが並行して `changePin` / `reset` を行うと pending キーを奪い合う。単一インスタンス制御（ロックファイル、`QLockFile` 等）をアプリ側で行うか決める
- **データ形式**: エントリ名・Vault ID を AAD に含めるか、Argon2 パラメータ更新時の再ラップ方針。`VLT2` で本番データが溜まる前に決める
- **libsecret スキーマ名**: `com.example.localvault.Vault` は仮。変更すると既存項目が見えなくなるため、本番前に確定させる（変更時は移行処理が必要）
- **IME 非対応の PIN 入力**: `SecurePinEdit` は IME を無効化している。IME 経由の文字を PIN に使う要件があるか決める
- **`PR_SET_DUMPABLE=0`**: `/proc/self/*` の所有者が root になる。Flatpak・ポータル連携・クラッシュレポータ等の配布形態で問題がないか確認する

#### 既知の制限

- `SecureBuffer::unlock()` はロック解除と同時に内容をゼロクリアする（`sodium_munlock` の仕様）。通常は `clear()` かデストラクタに任せ、`unlock()` を単独で呼ばないこと
- Argon2id（約1秒・256 MiB）が呼び出しスレッドで実行される。GUI では画面が固まるため、本番ではワーカースレッド（`QtConcurrent` 等）から呼ぶこと。Vault はスレッドセーフではないので、同時に複数スレッドから操作しないこと
- `runGuiDemo` は状態エラー時に `vault.state()` を再取得してメッセージを決めており、判定とメッセージが食い違う可能性がある（デモコードのみ）
- `SecurePinEdit` にはアクセシビリティ情報（ロール・名前）がない
- `SecurePinEdit` は Qt デザイナーの都合でグローバル名前空間に属している。`namespace localvault` 方針の例外である
- `--test` は実際の OS キーリングに `localvault-phase3-*` の項目を作成・削除する。CI では SystemKeychainBackend のテストが SKIP されるか、専用のセッションで実行すること
- Windows DPAPI バックエンドのキー無害化は `/` `\` `:` の置換のみ（FileBackend より緩い）
- libsecret 0.21 の `secret_password_search_sync` は、呼び出しごとに内部で 8 バイトのリークを起こす（LeakSanitizer で確認。こちらが受け取るオブジェクトはすべて解放済み）。長時間稼働で問題になる量ではない

## 推奨アーキテクチャ

### 鍵体系

```
┌─────────┐  Argon2id + salt  ┌─────┐
│   PIN   │ ─────────────────> │ KEK │
└─────────┘                    └─────┘
                                   │
┌─────────┐ XChaCha20-Poly1305    ▼
│    MK   │ <──────────────────  EMK
│(random) │                    (secure storage)
└────┬────┘
     │ XChaCha20-Poly1305
     ▼
[encrypted data file]
```

- **MK（Master Key）**: ランダムに生成された 256bit 鍵。実際の機密データを暗号化する
- **KEK（Key Encryption Key）**: PINからArgon2idで導出される32 byte鍵
- **EMK（Encrypted Master Key）**: KEKでXChaCha20-Poly1305認証付き暗号化されたMK
- EMK はセキュアストレージバックエンドに保存する
- PIN 変更時は EMK のみ再暗号化すればよく、全データの再暗号化は不要

### レイヤー構成

```
┌─────────────────────────────────────┐
│ GUI Layer (QtWidgets)               │
│  - PIN入力ダイアログ                  │
│  - Vaultの操作を呼び出す              │
├─────────────────────────────────────┤
│ Application Layer                   │
│  - Vault API                        │
├─────────────────────────────────────┤
│ Core Vault Module (Qt 非依存)       │
│  - SecureBuffer                     │
│  - libsodium (Argon2id / AEAD)      │
│  - Vault                            │
├─────────────────────────────────────┤
│ Storage Backends (Qt 非依存)        │
│  - SystemKeychainBackend            │
│  - FileBackend (fallback)           │
└─────────────────────────────────────┘
```

## 実装計画

### Phase 1: 暗号化基盤の整備

1. **SecureBuffer の実装**
   - `std::vector<uint8_t>` ベースのセキュアメモリコンテナ
   - 解放時に確実にゼロクリア（`sodium_memzero`/`SecureZeroMemory`/`memset_s` のような処理）
   - 可能であれば `mlock` / `VirtualLock` でメモリロック
   - Qt 依存なし

2. **AEAD / KDF の実装**
   - libsodiumのArgon2idとXChaCha20-Poly1305を使用する
   - 自作の暗号合成・タグ比較を行わない

### Phase 2: Vault コアの実装

1. **Vault クラスの設計・実装**
   - 2段階鍵体系（MK / KEK / EMK）を実装
   - 以下の API を提供
     - `VaultState state() const` — NotSetup / Locked / Unlocked / BackendUnavailable / StorageError
     - `VaultError setup(SecureBuffer const &pin)` — 初回セットアップ（既存EMKは上書きしない）
     - `VaultError unlock(SecureBuffer const &pin)` — PIN で Vault を解除
     - `void lock()` — メモリ上の MK を消去
     - `VaultError encrypt(SecureBuffer const &plain, Blob *cipher)` — データ暗号化（機密データはこちら）
     - `VaultError encrypt(Blob const &plain, Blob *cipher)` — データ暗号化（通常メモリ上のデータ用）
     - `VaultError decryptToSecureBuffer(Blob const &cipher, SecureBuffer *plain)` — データ復号API（平文はSecureBufferに返す）
     - `VaultError changePin(SecureBuffer const &oldPin, SecureBuffer const &newPin)` — PIN変更
     - `VaultError reset(SecureBuffer const &newPin)` — 新しいMKでEMKを置き換える不可逆操作
     - `VaultError destroy()` — EMKを削除する不可逆操作
    - 成功時は `VaultError::None`。`BackendUnavailable` / `StorageError` を `WrongPin` や `NotSetup` と混同しないこと
    - Qt 非依存（std::vector<char> / std::string / std::filesystem）

2. **セキュアストレージバックエンドの抽象インターフェース**
   - `ISecretStorageBackend` を定義
   - Qt 非依存


### Phase 3: ストレージバックエンドの実装

1. **SystemKeychainBackend**
   - Linux: libsecret（Secret Service API）
   - macOS: Keychain Services（Security.framework）
   - Windows: DPAPI（`CryptProtectData` / `CryptUnprotectData`）
   - プラットフォーム判定は `__linux__`, `__APPLE__`, `_WIN32` を使用

2. **FileBackend**
   - OSセキュアストレージが使えない環境のフォールバック
   - EMK をファイルに保存し、パーミッション `0600` を設定
   - `AtomicFile` により、一時ファイル（0600 / CREATE_NEW）→ fsync → 置換 rename でアトミックに書き込み

### Phase 4: GUI とアプリケーション統合

1. **PIN 入力ダイアログ**
   - `QtWidgets` を使用
   - Vault モジュールから分離し、必要に応じて `SecureBuffer` として PIN を渡す

2. **main.cpp の更新**
   - 旧 `localvault` モジュールの呼び出しを新しい `Vault` API に置き換え
   - または、テスト用のエントリーポイントとして残す

3. **旧モジュールの整理**
   - 旧難読化・独自暗号プリミティブは削除済み

### Phase 5: ビルド設定とテスト

1. **localvault.pro の更新**
   - `QT += core` を維持
   - プラットフォーム別のライブラリを追加
   - 新しいソースファイルを追加

2. **テストの追加**
   - 暗号化/復号のラウンドトリップテスト
   - 各バックエンドの store/load/remove テスト
   - PIN 変更後の復号テスト
   - 改ざんデータの検出テスト

3. **ドキュメントの更新**
   - 本ファイルの内容を都度更新
   - 必要に応じて README を作成

## 想定するファイル構成

```
localvault/
├── AGENTS.md                           # 本ファイル
├── README.md                           # ユーザー向けREADME
├── .gitignore
├── qmake/
│   └── localvault.pro                 # Qt qmake プロジェクト
└── src/
    ├── main.cpp                        # アプリエントリーポイント
    ├── vault/                          # QtCore のみのVaultコア
    │   ├── SecureBuffer.h/cpp
    │   ├── Vault.h/cpp
    │   ├── SecretStorage.h
    │   └── ProcessHardening.h/cpp
    ├── storage/                        # ストレージバックエンド
    │   ├── AtomicFile.h/cpp
    │   ├── FileBackend.h/cpp           # Phase 2 で追加
    │   └── SystemKeychainBackend.h/cpp # Phase 3 で追加
    └── gui/                            # GUIレイヤー（QtWidgets）
    │   ├── SecurePinEdit.h/cpp         # Phase 7 で追加
    │   ├── SetupVaultDialog.h/cpp/.ui  # Phase 9 で追加
    │   ├── UnlockVaultDialog.h/cpp/.ui # Phase 9 で追加
    │   ├── ResetVaultDialog.h/cpp/.ui  # Phase 9 で追加
    │   ├── ChangePinDialog.h/cpp/.ui   # Phase 9 で追加
    │   └── SecureStoreGUI.h/cpp        # Phase 9 で追加
    └── app/                            # アプリケーション層（Qt 非依存）
        └── BackendSelector.h/cpp
```

## ビルド手順

### 前提

- Qt 6（または Qt 5.12 以上）とlibsodium開発ファイルがインストールされていること
- 各プラットフォームの開発ツールが整っていること

### Linux

libsecret 等の開発ファイルが必要な場合があります。

```bash
# Ubuntu/Debian の例
sudo apt-get install libsodium-dev libsecret-1-dev libglib2.0-dev

# ビルド
cd qmake
qmake localvault.pro
make

# 既存Vaultを削除（不可逆）
./localvault --reset --yes
```

### macOS

```bash
cd qmake
qmake localvault.pro
make
```

### Windows

```bash
cd qmake
qmake localvault.pro
nmake  # または mingw32-make
```

## セキュリティ上の注意

- Vaultの新規データはXChaCha20-Poly1305 AEADで保護される。独自のChaCha20/HMAC合成を新規実装で使用しないこと
- Argon2idのコストはEMKヘッダに記録する。デフォルトは opslimit 3、memlimit 256 MiB（libsodium MODERATE 相当）。ヘッダ改ざんによる過大なメモリ・CPU消費を避けるため、実装は受入上下限を設けている
- 形式は `VLT2` / version 2。試作版のversion 1のEMK・暗号文には互換性がない
- Vaultのリセットは既存MKを失わせ、旧暗号文を復号不能にする。GUIでは確認・新PIN入力を必須とし、CLIでは`--reset --yes`を必須とする
- 復号後の機密情報（APIキー等）は `SecureBuffer` で管理し、使用後は即座に消去すること
- スワップファイルへの漏洩を防ぐため、可能な限りメモリロックを行うこと
- EMK を書き換える処理は必ず `Vault` 内のステージング経由（`writeEmk()`）で行い、バックエンドに「削除してから追加」する実装を追加しないこと。本キーの置換に失敗した場合は旧PIN・新PINのどちらでも解除でき、新PINで解除するとステージングが本キーへ昇格する
- 保存先は `BackendSelector` が設定ディレクトリの `storage-backend` に記録する。記録済みの保存先が使えない場合に他の保存先へフォールバックしてはならない。`--reset --yes` は記録も削除する
- OSセキュアストレージが利用できない場合の `FileBackend` は、PINの総当たりリスクをユーザーが承知の上で使う代替手段とし、新規使用時は同意を得ること。保存キーは英数字・`_`・`.`・`-` のみに無害化され、UNIX ではディレクトリ・ファイルのパーミッションを制限する
- `SecureBuffer` は `sodium_malloc` で確保するため、1 バッファごとにガードページ分のメモリを消費する。大量の小さなバッファを作らないこと
- PIN 入力には `QLineEdit` ではなく `SecurePinEdit` を使用する（IME・クリップボードは無効）
- 空 PIN の許可・禁止はコンパイル時に選択可能である。空 PIN を許可すると KEK が事実上不要になりセキュリティが著しく低下するため、通常は禁止して使用すること
- 全シンボルは `namespace localvault` に属する。GUI 層から libsodium を直接呼ばず、`SecureBuffer` の API を使うこと
- Linux の libsecret では、ロック中の項目を `NotFound` と扱ってはならない（新規セットアップへ誘導され既存 EMK を上書きする恐れがある）。`load()` は `SECRET_SEARCH_ALL` で列挙し、ロック中なら `Unavailable` を返す
- `main()` 冒頭で `hardenProcess()` を呼び、コアダンプを抑止する（Linux では ptrace アタッチも拒否される）
- Windows ではテスト出力を表示するため `CONFIG += console` でビルドする。リリース時は必要に応じて解除する

## 今後の検討事項

- 生体認証（Touch ID / Windows Hello）との統合
- ハードウェアセキュリティモジュール（TPM / Secure Enclave）の活用
- 外部キー保存サービス（1Password API 等）との統合
- タイムベースな暗号化鍵ローテーション
- 監査ログの記録

## 参考

- [RFC 8439 - ChaCha20 and Poly1305 for IETF Protocols](https://tools.ietf.org/html/rfc8439)
- [RFC 8018 - PKCS #5: Password-Based Cryptography Specification Version 2.1](https://tools.ietf.org/html/rfc8018)
- [OWASP Secrets Management Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Secrets_Management_Cheat_Sheet.html)
