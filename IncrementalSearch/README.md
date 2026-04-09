# MeCaSearch

この文書は Claude Sonnet 4.6 によって、自動生成されました。

## 概要

**MeCaSearch** は、MeCab（日本語形態素解析エンジン）を活用した**リアルタイム ローマ字カナ検索ライブラリ**です。

[migemo](https://github.com/koron/cmigemo) にインスパイアされており、ローマ字入力のまま日本語テキストをインクリメンタル検索できます。migemo との違いは、MeCab による形態素解析を用いて単語の**読み仮名を正確に取得**することで、より高い検索精度と速度を実現することを目指している点です。

---

## 特徴

- **ローマ字 → カタカナ変換**  
  ユーザーがローマ字で入力したクエリを自動的にカタカナに変換します。ヘボン式・訓令式・その他一般的な入力方式に幅広く対応しています。

- **形態素解析による正確な読み取得**  
  MeCab（ipadic）を使用してテキストを形態素解析し、各単語の読み仮名（カタカナ）を取得します。辞書ベースの解析により、同音異字や複合語にも対応できます。

- **リアルタイム検索向け設計**  
  インクリメンタルサーチへの組み込みを想定した軽量な API 設計になっています。

- **静的ライブラリとして提供**  
  MeCab のソースコードを内包した静的ライブラリ（`libmecasearch`）としてビルドされるため、システムへの MeCab インストールが不要です。

---

## 仕組み

```
ローマ字入力（例: "keisatu"）
        ↓
カタカナ変換（例: "ケイサツ"）
        ↓
対象テキストを形態素解析 → 各行の読み仮名列を生成
        ↓
読み仮名列とクエリを照合
        ↓
マッチした行を返す
```

---

## API

### クラス: `MeCaSearch`

```cpp
#include "MeCaSearch.h"
```

#### メソッド一覧

| メソッド | 説明 |
|----------|------|
| `bool open(const char *dicpath)` | MeCab 辞書ディレクトリを指定して初期化する |
| `void close()` | リソースを解放する |
| `std::vector<Part> parse(std::string_view const &line)` | テキスト行を形態素解析し、読み仮名パーツのリストを返す |
| `std::string convert_roman_to_katakana(std::string_view const &romaji)` | ローマ字文字列をカタカナに変換して返す |

#### 構造体: `MeCaSearch::Part`

```cpp
struct Part {
    size_t offset; // 元テキスト内のバイトオフセット
    size_t length; // 元テキスト内のバイト長
    std::string text; // 読み仮名（カタカナ）
};
```

#### 使用例

```cpp
MeCaSearch mecasearch;
mecasearch.open("/path/to/mecab-ipadic");

// ローマ字をカタカナに変換
std::string query = mecasearch.convert_roman_to_katakana("keisatu");
// → "ケイサツ"

// テキストを形態素解析して読み仮名を取得
std::vector<MeCaSearch::Part> parts = mecasearch.parse("警察が来た。");
for (auto const &part : parts) {
    // part.text に各形態素の読み仮名が入る
}
```

---

## ビルド方法

本プロジェクトは Qt qmake を使用しています。

### 依存関係

- Qt 6.x
- C++17 対応コンパイラ（GCC / Clang）
- MeCab 辞書（mecab-ipadic）※ `mecab/mecab-ipadic/` に内包

### ビルド手順

```bash
# ライブラリのビルド
qmake libmecasearch.pro
make
```

---

## ディレクトリ構成

```
MeCaSearch/
├── MeCaSearch.h        # ライブラリ公開ヘッダ
├── MeCaSearch.cpp      # コア実装（形態素解析・ローマ字変換）
├── libmecasearch.pro   # Qt qmake プロジェクトファイル（静的ライブラリ）
├── config.h            # ビルド設定
├── mecab/
│   ├── mecab/src/      # MeCab ソースコード（内包）
│   ├── mecab-ipadic/   # MeCab 辞書（ipadic）
│   └── mecab-jumandic/ # MeCab 辞書（jumandic）
└── build/              # ビルド成果物
```

---

## ライセンス

MeCab 本体および ipadic の各ライセンスに従ってください。

---

## 関連プロジェクト

- [MeCab](https://taku910.github.io/mecab/) — 日本語形態素解析エンジン
- [migemo](https://github.com/koron/cmigemo) — ローマ字のままインクリメンタル日本語検索
