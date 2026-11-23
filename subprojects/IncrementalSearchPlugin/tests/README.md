# インクリメンタル検索プラグインのテスト

Qt Test で、組み込み MeCab 辞書による読みの照合、元の文字列上の一致範囲、ローマ字変換を確認する。先に検索プラグインを同じ Qt・コンパイラー・ビルド構成でビルドする。

Linux の Release 構成では、リポジトリのルートから実行する。

```bash
mkdir -p _build-search-tests
cd _build-search-tests
qmake6 ../subprojects/IncrementalSearchPlugin/tests/IncrementalSearchTests.pro CONFIG+=release
make -j4
cd ..
_bin/IncrementalSearchTests
```

Windows では同じプロジェクトを `qmake` と `jom` でビルドし、`_bin/IncrementalSearchTests.exe` を実行する。Qt とプラグインの依存 DLL を `PATH` から見つけられるようにする。Debug の検索プラグインを使う場合はテストも `CONFIG+=debug` でビルドする。

このテストは Guitar を起動せず、ビルド済みの検索プラグインを直接読み込む。
