# Local Vault

ローカルデスクトップGUIアプリケーション向けの、APIキー等の機密情報を安全に管理するための実装サンプルです。

単純な難読化（XOR + 固定シード）から脱却し、libsodium の XChaCha20-Poly1305 AEAD と Argon2id を用いた暗号化ベースの Vault 機能へ移行・拡張したものです。

## 目的

- APIキー等の機密情報を、PIN入力を条件に安全に暗号化/復号する
- OSが提供するセキュアストレージを優先的に利用する
- OSセキュアストレージが利用できない環境では、暗号化ファイルによるフォールバックを提供する
- Vault コアは QtCore のみに依存し、GUI 部分は QtWidgets に限定する

## アーキテクチャ

### 2段階鍵体系

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

- **MK（Master Key）**: ランダム 256bit。実際の機密データを暗号化
- **KEK（Key Encryption Key）**: PIN からArgon2idで導出（opslimit 3、memlimit 256 MiB）
- **EMK（Encrypted Master Key）**: KEK で XChaCha20-Poly1305 により認証付き暗号化された MK。セキュアストレージバックエンドに保存
- PIN 変更時は EMK のみ再暗号化すればよく、全データの再暗号化は不要

### レイヤー構成

```
┌─────────────────────────────────────┐
│ GUI Layer (QtWidgets)               │
│  - SetupVaultDialog                   │
│  - UnlockVaultDialog                  │
│  - ResetVaultDialog                   │
│  - ChangePinDialog                    │
│  - SecurePinEdit                      │
│  - SecureStoreGUI                     │
├─────────────────────────────────────┤
│ Application Layer                   │
│  - Vault API                          │
├─────────────────────────────────────┤
│ Core Vault Module (QtCore only)     │
│  - SecureBuffer, libsodium             │
├─────────────────────────────────────┤
│ Storage Backends (QtCore only)      │
│  - SystemKeychainBackend              │
│  - FileBackend (fallback)             │
└─────────────────────────────────────┘
```

## ファイル構成

```
localvault/
├── AGENTS.md                    # エージェント向け詳細設計書
├── README.md                    # 本ファイル
├── qmake/
│   └── localvault.pro          # Qt qmake プロジェクト
└── src/
    ├── main.cpp                 # テストランナー / GUI デモ
    ├── vault/                   # Qt 非依存の Vault コア
    │   ├── SecureBuffer.h/cpp
    │   ├── Vault.h/cpp
    │   ├── SecretStorage.h
    │   └── ProcessHardening.h/cpp
    ├── storage/                 # ストレージバックエンド（Qt 非依存）
    │   ├── AtomicFile.h/cpp
    │   ├── FileBackend.h/cpp
    │   └── SystemKeychainBackend.h/cpp
    ├── app/                     # アプリケーション層（Qt 非依存）
    │   └── BackendSelector.h/cpp
    └── gui/                     # GUI レイヤー（QtWidgets）
        ├── SecurePinEdit.h/cpp
        ├── SetupVaultDialog.h/cpp/.ui
        ├── UnlockVaultDialog.h/cpp/.ui
        ├── ResetVaultDialog.h/cpp/.ui
        ├── ChangePinDialog.h/cpp/.ui
        └── SecureStoreGUI.h/cpp
```

## 他のアプリからの組み込み

`qmake/localvault.pri` を自分のプロジェクトの `.pro` から include することで、必要なソース・フォーム・ライブラリ設定を一括で読み込めます。include 前に `LOCALVAULT_SRC` に `localvault/src` へのパスを設定してください。

```pro
LOCALVAULT_SRC = /path/to/localvault/src
include(/path/to/localvault/qmake/localvault.pri)
```

## ビルド手順

### 前提

- Qt 5.12 以上、または Qt 6
- libsodium の開発ファイル
- Linux では libsecret / glib の開発ファイル

### Linux

```bash
# Ubuntu/Debian の例
sudo apt-get install libsodium-dev libsecret-1-dev libglib2.0-dev

# ビルド
cd qmake
qmake localvault.pro
make

# テスト実行
./localvault --test

# GUI デモ実行
./localvault

# 既存Vaultを削除（不可逆。次回GUI起動時に再セットアップ）
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

## クラスの使い方

### SecureBuffer

機密データを保持するコンテナ。内部領域は `sodium_malloc`（ガードページ付き）で確保し、解放時・再確保時に旧領域をゼロクリアする。`lock()` で明示的にメモリロックでき、ロック中は再配置を招く`resize`、`assign`、`append`を拒否する。

```cpp
#include "vault/SecureBuffer.h"

SecureBuffer buf(32);                          // 32バイト確保
std::fill(buf.begin(), buf.end(), 0xab);       // データ書き込み
buf.lock();                                    // スワップ防止（可能な場合）
// ... 使用 ...
buf.clear();                                   // 明示的に消去
```

### Vault

Vault は `ISecretStorageBackend` を通じて EMK を保存する。保存先は `BackendSelector` で選択・記録する。
各操作は `VaultError` を返し、成功時は `VaultError::None`。すべてのクラス・関数は `namespace localvault` に属する（以下の例では `using namespace localvault;` を前提とする）。

```cpp
#include "app/BackendSelector.h"
#include "vault/Vault.h"
#include "vault/SecureBuffer.h"

// 保存先の選択：記録済みならその保存先のみ。未記録なら既存Vaultを探し、なければOSストレージを優先
BackendSelector selector(configDir, "com.example.myapp.Vault", "my-app-emk");
BackendSelection sel = selector.select(false);
switch (sel.status) {
case BackendSelection::Status::Ok: break;
case BackendSelection::Status::NeedsFileConsent: /* ユーザーの同意を得たら続行 */ break;
case BackendSelection::Status::RecordedBackendUnavailable: /* 再試行を促す。他の保存先へ切り替えない */ return;
case BackendSelection::Status::ConfigError: return;
}

Vault vault(sel.backend.get(), "my-app-emk");

SecureBuffer pin;
pin.assign("1234", 4);
switch (vault.state()) {
case VaultState::NotSetup:
    if (vault.setup(pin) == VaultError::None && !sel.recorded) {
        selector.record(sel.kind);   // セットアップ成功後に保存先を記録
    }
    break;
case VaultState::Locked:
    if (vault.unlock(pin) == VaultError::WrongPin) { /* 再入力 */ }
    break;
case VaultState::BackendUnavailable:
case VaultState::StorageError:
    // EMKの有無が不明。新規セットアップへ進まないこと
    break;
case VaultState::Unlocked:
    break;
}

// 機密データの暗号化/復号（平文は SecureBuffer で渡し、通常メモリを経由させない）
SecureBuffer secret;
secret.assign("API_KEY", 7);
Blob encrypted;
if (vault.encrypt(secret, &encrypted) == VaultError::None) {
    SecureBuffer decrypted;
    if (vault.decryptToSecureBuffer(encrypted, &decrypted) == VaultError::None) {
        // 使用後は decrypted.clear() で明示的に消去
    }
}

// PIN 変更（StorageError の場合、旧PIN・新PINのどちらかが有効）
SecureBuffer newPin;
newPin.assign("5678", 4);
VaultError err = vault.changePin(pin, newPin);

// 新しいMKでEMKを置き換える（不可逆。旧暗号文は復号不能になる）
err = vault.reset(newPin);

// 使用後はロック
vault.lock();
```

`VaultError` の主な値:

| 値 | 意味 |
|---|---|
| `WrongPin` | PIN の誤り（EMK 改ざんでも同じ結果） |
| `BackendUnavailable` | ストレージに到達できない・ロックされている。リセットへ誘導しないこと |
| `StorageError` | ストレージの読み書き・検証失敗 |
| `CorruptedData` | 形式不正・未対応バージョン |
| `AuthenticationFailed` | 暗号文の改ざん、または別のMKで暗号化されたデータ |
| `MemoryLockFailed` | 鍵用メモリのロック失敗（`RLIMIT_MEMLOCK`） |

### GUI ダイアログ

Vault 操作専用の QtWidgets ダイアログ群。Vault から独立しており、入力欄は `SecurePinEdit` で、入力を `QString` に保持せず `SecureBuffer` に直接格納する（IME・クリップボード・カーソル移動は無効）。

- `SetupVaultDialog`: 新規 Vault セットアップ（2回入力で確認）
- `UnlockVaultDialog`: Vault 解除
- `ResetVaultDialog`: Vault リセット（2回入力で確認）
- `ChangePinDialog`: PIN 変更

```cpp
#include "gui/SetupVaultDialog.h"

SetupVaultDialog dlg(parent);
if (dlg.exec() == QDialog::Accepted) {
    SecureBuffer pin;
    if (dlg.pin(&pin)) {
        VaultError err = vault.setup(pin);
    }
}
```

### SecureStoreGUI

保存先選択・セットアップ・解除・リセットをまとめて行うユーティリティクラス。アプリケーション側で `Vault` と `BackendSelector` の繋ぎ込みを簡略化する用途を想定している。

```cpp
#include "gui/SecureStoreGUI.h"

// OS のセキュアストレージを優先する既定設定
localvault::SecureStoreGUI store(parent);
localvault::VaultWithBackend vault = store.execUnlock(configDir, schema);
if (vault) {
    // vault.vault を通じて encrypt/decrypt 等を実行
}
```

アプリケーションの要件として常にファイル保存を使う場合は、生成時または
`setStoragePreference()` で `StoragePreference::FileOnly` を指定する。この設定は
OS のセキュアストレージや既存の保存先記録を探索せず FileBackend を選ぶため、Vault を
作成した後に切り替えてはならない。PIN 変更ダイアログを使用する場合も、同じ
`StoragePreference` をコンストラクタへ渡すこと。

```cpp
localvault::SecureStoreGUI store(parent, localvault::StoragePreference::FileOnly);
```

## 形式と互換性

EMK と暗号文には、magic、形式バージョン、レコード種別、暗号方式、KDF方式・計算/メモリコスト（EMKのみ）を含め、ヘッダ全体をAEADの追加認証データとして保護する。現行形式は `VLT2` / version 2 である。

EMK のヘッダには Argon2id の opslimit / memlimit が記録される。デフォルトは opslimit 3、memlimit 256 MiB（libsodium の MODERATE 相当）である。PIN 変更時も新しい salt と共にこれらのパラメータが再設定される。

### EMK の書き換え

`setup()` / `changePin()` / `reset()` は、まず `<emkKey>.pending` に新EMKを書き込んで読み戻し検証し、その後で本キーを置き換える。本キーの置換に失敗しても旧EMK・新EMKのどちらかは必ず残り、旧PIN・新PINのどちらでも解除できる（新PINで解除した場合はステージングが本キーへ昇格し、旧PINで解除した場合はステージングが破棄される）。

初期の試作版（ChaCha20 + HMAC、version 1）のEMKおよび暗号文とは互換性がない。試作版のデータを利用している場合は、その版で復号してから本版で再セットアップ・再暗号化すること。新規の`setup()`は既存EMKを上書きしない。

## Vaultのリセット

リセットは新しいMKでVaultを作り直す不可逆操作である。新EMKの書き込みに成功するまで既存EMKは削除しない。旧MKで暗号化したデータは復号できなくなるため、必要なデータは先に復号・退避すること。

- GUIでは、既存Vaultを検出した際に「Reset Vault」を選択し、警告確認後に新PINを設定する。
- CLIでは`./localvault --reset --yes`を実行する。CLI操作はEMKと保存先の記録を削除し、次の通常起動時にGUIで新規セットアップを行う。`--yes`なしでは何も削除しない。記録済みの保存先にアクセスできない場合は何も削除せず終了コード1を返す。
- アプリケーションコードからは`Vault::reset(SecureBuffer const &newPin)`、または削除のみなら`Vault::destroy()`を使用する。

## 保存先の選択

`BackendSelector` は使用した保存先（`system` / `file`）を設定ディレクトリの `storage-backend` ファイルに記録し、以後はその保存先のみを使う。OSストレージが一時的に使えない（キーリングのロック、D-Bus未起動等）場合でも FileBackend へ切り替えず、再試行を求める。

記録がない場合は、OSストレージ・ファイルの順に既存Vaultを探し、見つかればそれを採用・記録する（旧バージョンからの移行）。既存Vaultがなく、OSストレージも使えない場合は、GUIでPIN総当たりのリスクを説明した上でファイル保存への同意を求める。

### バックエンドの直接利用

```cpp
#include "storage/SystemKeychainBackend.h"
#include "storage/FileBackend.h"

SystemKeychainBackend backend;
if (backend.isAvailable()) {
    Blob data = { 'e', 'n', 'c', 'r', 'y', 'p', 't', 'e', 'd', '-', 'b', 'l', 'o', 'b' };
    backend.store("my-key", data);              // 既存データはアトミックに置き換えられる
    Blob loaded;
    StorageStatus status = backend.load("my-key", &loaded); // Ok / NotFound / Unavailable / Error
    backend.remove("my-key");
}
```

## プラットフォームごとのセキュアストア動作の流れ

### Linux: libsecret（Secret Service API）

```
[Vault]
   │ store(EMK)
   ▼
[SystemKeychainBackend]
   │ Base64 エンコード
   ▼
[libsecret]
   │ D-Bus Secret Service
   ▼
[GNOME Keyring / KDE Wallet / keepassxc-secret-service 等]
```

- `secret_password_store_sync` でデフォルトコレクションに保存（同じ属性の既存項目は置き換えられる）
- 属性 `key` を使って項目を識別
- `isAvailable()` は Secret Service への接続とコレクション一覧の取得のみを行い、解除ダイアログは出さない
- 読み出しは `secret_password_search_sync` で行い、ロック中の項目も列挙する。解除ダイアログがキャンセルされ項目がロックされたままの場合は「存在しない」ではなく `Unavailable` を返す（既存 Vault を新規作成で上書きしないため）
- キーリングがロックされている場合、保存/読み出し時に解除ダイアログが表示される

### macOS: Keychain Services

```
[Vault]
   │ store(EMK)
   ▼
[SystemKeychainBackend]
   ▼
[Security.framework]
   │ SecItemUpdate（なければ SecItemAdd）/ SecItemCopyMatching / SecItemDelete
   ▼
[macOS Keychain]
```

- `kSecClassGenericPassword` として保存
- `kSecAttrService` にアプリ識別子、`kSecAttrAccount` にキー名を使用
- ユーザーがキーチェーンをロックしている場合、OS のダイアログで解除を求められる

### Windows: DPAPI + ローカルファイル

```
[Vault]
   │ store(EMK)
   ▼
[SystemKeychainBackend]
   │ CryptProtectData（ユーザー資格情報で暗号化）
   ▼
[ローカルファイル]
   │ %AppData%/<appname>/emk/<key>
```

- DPAPI で EMK を暗号化（保護対象は現在ログイン中のユーザー）
- 暗号化済みバイナリを `%AppData%` 以下に保存
- 一時ファイル → `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)` でアトミックに置き換える
- ファイルパーミッションは OS に委ねる（必要に応じて追加設定）
- ログイン中のユーザーでないと復号できない

## セキュリティ上の注意

- 本実装はあくまで実験・サンプルコードです。本番利用の前にセキュリティレビューを行ってください。未検証事項・未決定の仕様・既知の制限は `AGENTS.md` の「既知の課題・未決事項」を参照してください。
- 機密データの暗号化には `encrypt(SecureBuffer const &, Blob *)` を使用してください。
- Argon2id（約1秒・256 MiB）は呼び出しスレッドで実行されます。GUI アプリではワーカースレッドから呼び出してください。`Vault` はスレッドセーフではありません。
- 同一ユーザーが複数インスタンスを同時に起動して PIN 変更等を行うことは想定していません。必要に応じてアプリ側で単一インスタンス制御を行ってください。
- FileBackend で作成した Vault は、後から OS ストレージが使えるようになっても自動では移行されません。
- KEKはArgon2id（opslimit 3、memlimit 256 MiB）で導出します。コストはEMKに格納され、受入値には上下限があります。
- 復号には平文を通常メモリへコピーしない`decryptToSecureBuffer()`を使用してください。
- `FileBackend` は OS セキュアストレージが使えない場合に、PIN 総当たりのリスクを承知の上で使う代替手段です。書き込みは 0600 の一時ファイル → fsync → rename で行います。保存キーは英数字・`_`・`.`・`-` のみに無害化され、ディレクトリ・ファイルのパーミッションは可能な範囲で制限されます。
- `StoragePreference::FileOnly` は、アプリケーションが明示的に FileBackend を常用する場合の設定です。このモードではフォールバック確認を表示せず、OS のセキュアストレージや保存先記録も探索しません。初回セットアップ前に固定し、既存 Vault の保存先を切り替える目的には使用しないでください。
- 復号後の機密情報は `SecureBuffer` で管理し、使用後は即座に消去してください。
- `SecurePinEdit::pin()` と各 PIN ダイアログの `pin()` は、メモリロック済みの PIN を出力引数に返します。false の場合は PIN を使用せず、メモリロック失敗として処理してください。
- アプリケーションの `main()` 冒頭で `hardenProcess()` を呼び、コアダンプを抑止してください（Linux では同一ユーザーからの ptrace アタッチも拒否されます）。
- `SecureBuffer::unlock()` はロック解除と同時に内容をゼロクリアします。単独で呼ばず、`clear()` かデストラクタに任せてください。
- PIN 入力欄（`SecurePinEdit`）は IME に対応していません。
- 空 PIN の許可・禁止はコンパイル時に選択可能です。空 PIN を許可すると KEK が事実上不要になり、Vault の保護が著しく弱まるため、通常は禁止してください。
- 可能な限りメモリロック（`mlock` / `VirtualLock`）を利用し、スワップファイルへの漏洩を防いでください。
- Windows ではテスト出力を表示するためコンソールサブシステムでビルドされます。リリース時は必要に応じて `qmake/localvault.pro` の `CONFIG += console` を削除してください。

## ライセンス

このプロジェクトは実験的なサンプルコードです。ライセンスは別途指定がない限り、プロジェクトの所有者に帰属します。
