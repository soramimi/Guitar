# LineIndexMap

## プロジェクトの目的

折り返し(ワードラップ)機能付きテキストエディタで、**論理行番号と表示行番号を相互変換する**ためのデータ構造の試作。

- 要素のインデックス(キー)= 論理行番号(**0ベース**)
- 値 = `ValueItem`。その論理行が折り返しで占める**表示行ごとに `UserData` を1要素**持ち、
  その個数(`value()`)が折り返し数を兼ねる。`UserData::vcol_len` に各折り返し行の
  桁数を持たせると、論理列まで含めた座標変換ができる
- `count(i)` = 論理行 0..i-1 の表示行数の合計 = **論理行 i の先頭の表示行インデックス**(0ベース)
- 総表示行数は `count(論理行数)` で取得できる
- `logical_to_visual(行, 列)` が順変換、`visual_to_logical(表示行)` が逆変換

0ベース + 半開区間 [0, key) の組み合わせにより、`count(0) == 0` が自然に成り立ち、±1 の補正が不要。

## ビルド

qmake プロジェクト(Qt Creator)。C++17、コンソールアプリ、Qt ライブラリには依存しない(`CONFIG -= qt`)。

`LineIndexMap` クラスは `LineIndexMap.h`(ヘッダオンリー)。`main.cpp` は GoogleTest の
テストスイートで、実行ファイル自体がテストランナー(`main()` は gtest_main が提供)。
システムの gtest を `-lgtest -lgtest_main` でリンクする。

```
cd build/Qt_6_9_0_Debug && qmake6 ../../LineIndexMap.pro && make && ./LineIndexMap
# 期待出力: [  PASSED  ] 22 tests.
```

## データ構造の設計

**固定3階層の B+-tree**: root(`std::vector<Node> nodes_`)→ Node → Leaf。
容量は 256×256×256 ≒ 1,677万行(root は可変長なので実際は上限なし)。

- `Node` は `std::vector<Leaf> leaves`(最大 `max_node_fanout` = 256)と、
  配下の総 item 数 `num_items`(キー解決のスキップ用)、配下の `value()` 総和
  `sum_values`(count / 逆変換のスキップ用)をキャッシュする
- キー解決は Node → Leaf → item の3段: `key >= num_items` の Node は丸ごとスキップ
- Leaf 分割で Node の fanout を超えたら Node も半分に分割(`split_node`)。
  末尾追加・隙間埋めは `ensure_tail` が「最後の Node の最後の Leaf」を詰めて構築する
  (末尾経路では分割は起きず、満杯まで詰まる)
- 各操作は O(root内Node数 + fanout + 葉容量)。100万行で count/update/insert とも
  1段構造の2〜5倍高速(実測)

- `value_type` = `ValueItem`: `std::shared_ptr<std::vector<UserData>> data_`(private、
  `LineIndexMap` のみ friend)を持ち、`value()` は `data_->size()` を返す。
  `ValueItem(uint32_t n)` で UserData n 個入り(vcol_len 未設定)、
  `ValueItem(std::vector<uint32_t> const &col_count_list)` で各折り返し行の桁数付きで構築。
  デフォルト構築は空ベクタ(value 0)。**コピーは `shared_ptr` の浅い共有**
- `ValueItem::locate_column(lcol)` が「論理列 → (折り返し行, 行内列)」、逆演算の
  `column_of_row(row)` が「折り返し行 → 先頭論理列」の解決を担う。
  折り返し境界の知識は ValueItem に閉じ、B+-tree 側は内部表現を知らない
- `Leaf` は `std::vector<value_type> items`(最大 `max_leaf_capacity` = 256 要素)と、
  その葉の `value()` の合計 `uint64_t sum_values` を持つ
- キーは葉をまたいだ**通算インデックス**。先頭の葉から要素数を差し引きながら解決する
- 挿入で葉のサイズが `max_leaf_capacity` を超えたら、葉を半分に分割して新しい葉を直後に挿入する
  (private ヘルパー `insert_into_leaf` が一手に引き受ける)
- `key_type` は `uint32_t`。`sum_values` と `count` の戻り値はオーバーフロー回避のため `uint64_t`
- `Leaf` / `Node` / `root_` / `max_leaf_capacity` は private。外部 API は
  find / count / insert / update / erase のみ

### 不変条件

- 各葉のサイズは `max_leaf_capacity` 以下、各 Node の葉数は `max_node_fanout` 以下
- 各葉の `sum_values` は `items` の `value()` の総和と、各 Node の `num_items` /
  `sum_values` は配下の葉の集計と常に一致する(update/insert/erase/分割のすべてで維持)
- 空の Leaf は Node から、空の Node は root から erase 時に取り除かれる
- `ValueItem::data_` は null にならない(`assert(data_)`)
- 公開メソッド `validate()` が上記すべてを検査する(テスト用。true なら整合)

### 既知のリスク

- `ValueItem::data_` は private のため外部からは増減できず、折り返し数の変更は
  必ず `update` を通る(「外部から `sum_values` の整合が破れる」リスクはカプセル化で解消済み)
- 同じ `ValueItem` を複数キーに `update` すると `UserData` ベクタが共有される(浅いコピー)
- 値 0 の要素(デフォルト構築)でも `make_shared` のヒープ確保が走る。
  insert の隙間 0 埋め n 個 = n 回のアロケーション

## API とセマンティクス

キーは「安定した ID」ではなく**位置**。実体はマップではなく挿入・削除可能な列(シーケンス)。

| 操作 | 挙動 |
|---|---|
| `find(key)` | key 位置の `ValueItem` を `std::optional` で返す。範囲外は `nullopt` |
| `count(key)` | **[0, key) の `value()` の合計**。葉全体が範囲に収まる場合は `sum_values` で葉ごとスキップし O(葉数 + 葉容量)。key が総要素数を超えたら全体の合計 |
| `insert(key, item)` | key の位置に**挿入**し、後続のキーは 1 つ後ろへずれる。key が末尾より先なら隙間をデフォルト構築(value 0)で埋めてから配置 |
| `update(key, item)` | 既存キーなら**上書き**(シフトなし)。最大キーを超えていたら**末尾に追加**(隙間埋めはしない)。連続 append にも使う |
| `erase(key)` | key 位置の要素を削除し、後続のキーは 1 つ前へ詰まる |
| `logical_to_visual(lrow, lcol)` | **(論理行, 論理列) → `VisualPosition{vrow(uint64_t), vcol}`**。行頭の表示行(count)に、論理列が属する折り返し行のオフセット(`locate_column`)を加え、vcol は折り返し行内の列。列 == 桁数の境界は次の折り返し行頭へ、行末超過は最終折り返し行に丸め、vcol_len 未設定(0)の行は境界にならない。論理行が範囲外なら {総表示行数, 0} |
| `visual_to_logical(vrow)` | count の逆変換。`count(i) <= vrow < count(i+1)` となる **`LogicalPosition{lrow, lcol, wrap_index}`** を返す(wrap_index = 行内で何番目の折り返し行か、lcol = その折り返し行の先頭論理列)。Node → Leaf → item の順に `sum_values` でスキップ。value 0 の行は表示行を持たないためスキップされる。総表示行数以上は lrow = 総論理行数(末尾の次) |
| `clear()` | 全消去(画面幅変更時の全再構築などに使う) |

内部ヘルパ `count_and_find(key)` が「[0, key) の前置和 + key 位置の item ポインタ」を
一度の走査で返し、`count` と `logical_to_visual` が共有する(二度引き防止)。
`total_logical_row_count()` / `total_visual_row_count()` は Node の集計値の合計を
返す O(Node数) の総数 API(スクロールバーのレンジ計算などに使う)。

注意: エディタ用途では空行も 1 表示行を占めるため、値は原則 1 以上(UserData 1 個以上)を
格納する想定。insert の隙間埋めで value 0 の要素が混ざると、表示行 → 論理行の逆変換が
曖昧になるので注意。

## テスト方針

`main.cpp` に GoogleTest のテストスイートがある
(公開 API + `validate()` のみ使用。内部は private のため直接は触らない):

- 境界値: 空マップ、隙間0埋め、insert のシフト、update の非シフト・末尾追加、
  erase のシフト・範囲外 no-op・全削除、count の前置和と範囲外
- 分割: Leaf 分割(300要素)、Node 分割(7万要素 + 中間挿入で連鎖分割)
- **オラクル比較**: `std::vector<uint32_t>` を正とし、ランダム1万操作後に
  全キーの find / count を照合
- 逆変換: 基本対応表([1,3,1,2] の全表示行)、value 0 のスキップ、
  ランダム構築(value 0 含む)での全表示行往復、Node 分割規模(7万行)での往復
- 順変換: `locate_column` / `column_of_row` の境界(桁数ちょうど・行末超過・vcol_len 未設定)、
  `logical_to_visual` の vrow/vcol の解決と逆変換との往復、`visual_to_logical` の lcol、
  範囲外規約(末尾の次)、総数 API、`clear` 後の再構築

開発時はこれに加えて `/tmp` の一時テストで、小容量コピー(leaf 4 / fanout 4、2 / 2)の
ストレステストと複数シードのオラクル比較も実施した。容量を変えた検証を常設したければ
`max_leaf_capacity` / `max_node_fanout` をテンプレートパラメータ化する手がある。

ベンチマーク(-O2、100万行、fanout 無制限の1段相当との比較、2026-09実測):
count ×10万: 123ms vs 267ms、update ×10万: 37ms vs 189ms、
insert/erase ×2万: 35ms vs 142ms。

## 今後の予定

3階層 B+-tree 化(2026-09)と逆変換 `visual_to_logical`(2026-09)は実装済み。
論理行 → 表示行(count)と表示行 → 論理行(visual_to_logical)の双方向変換が揃い、
さらに論理列を含む座標変換(`VisualPosition` / `LogicalPosition`、UserData::vcol_len ベース)と
総数 API(`total_logical_row_count` / `total_visual_row_count`)も実装済み(2026-09)。

### 改良案

- **Node のリバランス**: 3階層化ではアンダーフロー時の隣接マージを実装しない予定。
  単調な削除が続くと fanout が痩せるため、必要になったらマージを追加する
- **全再構築の高速経路**: 画面幅変更時は全行の折り返し数が変わる。クリア後に
  `update` の末尾追加を繰り返せば O(N) で再構築できるが、専用の一括構築
  (`assign` / `rebuild`)を用意するとより明快
- **総数 API の O(1) 化**: `total_logical_row_count` / `total_visual_row_count` は
  現在 O(Node数)。呼び出し頻度が高ければ root レベルにキャッシュを持つ
- **value 0 のアロケーション回避**: `data_` が null なら value() は 0 とみなす設計にすれば、
  隙間埋めのヒープ確保をなくせる(現状は `assert(data_)` で null 禁止)
- **erase の一括版**: 複数行削除(範囲 erase)を 1 要素ずつの erase より効率的に行う
- **容量のテンプレートパラメータ化**: 小容量での分割ストレステストを常設できるようにする
