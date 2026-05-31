# tooluse

MCP (Model Context Protocol) の tool_use 機能をテストするための実験的な CLI ツールです。  
[Guitar](../../README.md) プロジェクトの `extra/chat` コマンドをベースに、マルチターン会話と tool_use のテスト機能を追加した派生プロジェクトです。

---

## 概要

`AiApiBridge::query2()` を使用したマルチターン会話と、AI API の tool_use (関数呼び出し) 機能の動作検証を目的とした実験的な実装です。  
`chat` コマンドと同じ API ブリッジ層 (`AiApiBridge` / `GenerativeAI`) を使用しているため、Guitar と同じ AI プロバイダおよびモデルをそのまま利用できます。

### chat コマンドからの主な変更点

| 変更点 | 内容 |
|---|---|
| マルチターン会話 | `AiApiBridge::query2()` により複数プロンプトを順次送信する会話フローを実装 |
| tool_use サポート | `tools` 配列と `tool_choice` を含む JSON リクエストの構築・送信 |
| JSON 直接送信モード | `Quert2Resuest::JSON` 型によるロー JSON リクエストの送信（実験的、現在 `#if 0` で無効） |
| プロンプト入力 | CLI 引数・標準入力によるプロンプト指定は現在無効化（`#if 0`）。コード内に固定プロンプトを記述して実行 |

現在のデフォルト動作は "Hello" / "Who are you?" / "Write a haiku." の 3 ターン会話を送信します。

---

## ビルド

### 依存ライブラリ

- Qt 6 (core のみ、GUI 不要)
- libcurl
- zlib

### ビルド手順

```sh
cd extra/tooluse
qmake tooluse.pro
make
```

ビルド成果物は `_bin/tooluse` に出力されます。

---

## 使い方

引数なしで実行します（プロンプトは `main.cpp` 内に直接記述）。

```sh
_bin/tooluse
```

`run.sh` はこの使い方のサンプルスクリプトです。

---

## 設定

設定ファイルは INI 形式で、起動時に自動的に読み込まれます。

| OS | パス |
|---|---|
| Linux / macOS | `~/.config/soramimi.jp/chat/chat.ini` |
| Windows | `%LOCALAPPDATA%\soramimi.jp\chat\chat.ini` |

### 設定ファイルの例

```ini
[AI]
model = claude-sonnet-4-6
```

---

## API キー

API キーは環境変数で指定します。使用するプロバイダに対応する変数を設定してください。

| プロバイダ | 環境変数 |
|---|---|
| OpenAI | `OPENAI_API_KEY` |
| Anthropic (Claude) | `ANTHROPIC_API_KEY` |
| Google (Gemini) | `GOOGLE_API_KEY` |
| xAI (Grok) | `XAI_API_KEY` |
| さくら AI Engine | `SAKURA_AI_API_KEY` |
| DeepSeek | `DEEPSEEK_API_KEY` |
| OpenRouter | `OPENROUTER_API_KEY` |
| llama.cpp | `LLAMACPP_API_KEY` |
| Ollama / LM Studio | 不要（ローカル動作） |

---

## 対応モデル (プリセット)

| モデル名 | プロバイダ |
|---|---|
| `gpt-5.5` | OpenAI |
| `gpt-5.4-mini` | OpenAI |
| `gpt-5.4-nano` | OpenAI |
| `gpt-5.3-codex` | OpenAI |
| `claude-opus-4-7` | Anthropic |
| `claude-sonnet-4-6` | Anthropic |
| `claude-haiku-4-5` | Anthropic |
| `gemini-3-flash-preview` | Google |
| `grok-4.20` | xAI |
| `sakura:gpt-oss-120b` | さくら AI Engine |
| `deepseek-chat` | DeepSeek |
| `openrouter:///anthropic/claude-4.6-sonnet` | OpenRouter |
| `ollama:///gemma4` | Ollama |
| `lmstudio:///meta-llama-3-8b-instruct` | LM Studio |
| `llamacpp://localhost:8080/` | llama.cpp |

プリセット以外のモデルも、プロバイダのプレフィックス規則に従った名前であれば指定できます（例: `gpt-` で始まるモデルは OpenAI として扱われます）。

---

## Guitar 本体から流用しているコード

| ファイル | 役割 |
|---|---|
| `src/AiApiBridge.cpp/h` | AI API 呼び出しブリッジ層（`query2()` によるマルチターン対応） |
| `src/GenerativeAI.cpp/h` | 生成 AI モデル管理・API リクエスト送信 |
| `src/curlclient.cpp/h` | HTTP 通信（libcurl ラッパー） |
| `src/inetclient.cpp/h` | ネット通信抽象インターフェース |
| `src/FileTypeDetector.cpp/h` | ファイルタイプ検出 |
| `src/common/` 各種 | 文字列・パス・URL エンコード等ユーティリティ |
| `extra/common/` 各種 | INI 設定パーサー・ファイル読み書き |
