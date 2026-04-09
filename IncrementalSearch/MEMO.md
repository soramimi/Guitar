# MeCab 辞書ファイル メモ

## 実行時に必要な辞書ファイル

| ファイル | 種別 | 説明 |
|---|---|---|
| `dicrc` | テキスト | 辞書設定ファイル |
| `sys.dic` | バイナリ | システム辞書 |
| `unk.dic` | バイナリ | 未知語辞書 |
| `matrix.bin` | バイナリ | 品詞間連接コスト行列 |
| `char.bin` | バイナリ | 文字カテゴリ定義 |

※ `*.bin` / `*.dic` の4ファイルは `mecab-dict-index` によってビルド時に生成される。

## ソースコード上の読み込み箇所

### dicrc
- **ファイル**: `mecab/src/utils.cpp`
- **関数**: `load_dictionary_resource()`
- 辞書ディレクトリの `dicrc` を読み込み、パラメータを初期化する。

### sys.dic
- **ファイル**: `mecab/src/tokenizer.cpp`
- **関数**: `Tokenizer::open()`
- `create_filename(prefix, SYS_DIC_FILE)` でパスを生成して `Dictionary::open()` で読み込む。

### unk.dic
- **ファイル**: `mecab/src/tokenizer.cpp`
- **関数**: `Tokenizer::open()`
- `create_filename(prefix, UNK_DIC_FILE)` でパスを生成して読み込む。

### matrix.bin
- **ファイル**: `mecab/src/connector.cpp`
- **関数**: `Connector::open()`
- `create_filename(dicdir, MATRIX_FILE)` でパスを生成して読み込む。

### char.bin
- **ファイル**: `mecab/src/char_property.cpp`
- **関数**: `CharProperty::open()`
- `create_filename(prefix, CHAR_PROPERTY_FILE)` でパスを生成して読み込む。

## 共通事項

- ファイル名のマクロ定義は `mecab/src/common.h` に集約されている。
- バイナリファイルはすべて `mecab/src/mmap.h` の `Mmap<T>::open()` を通じて **mmap** でメモリマップされる。
- 辞書ディレクトリのパスは `dicrc` の `dicdir` パラメータで決まる。