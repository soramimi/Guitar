#include <gtest/gtest.h>
#include <vector>
#include <random>
#include "somethingmap.h"

// key位置の値(折り返し数)を取得するヘルパ
static std::optional<uint32_t> value_at(SomethingMap const &map, uint32_t key)
{
	auto opt = map.find(key);
	if (!opt) return std::nullopt;
	return opt->value();
}

TEST(SomethingMap, EmptyMap)
{
	SomethingMap map;
	EXPECT_FALSE(map.find(0));
	EXPECT_FALSE(map.find(100));
	EXPECT_EQ(map.count(0), 0u);
	EXPECT_EQ(map.count(100), 0u);
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, InsertAtZero)
{
	SomethingMap map;
	map.insert(0, 5);
	EXPECT_EQ(value_at(map, 0), 5u);
	EXPECT_FALSE(map.find(1));
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, InsertWithGapFill)
{
	// 末尾より先へのinsertは隙間をvalue 0で埋める
	SomethingMap map;
	map.insert(12, 34);
	EXPECT_EQ(value_at(map, 12), 34u);
	for (uint32_t k = 0; k < 12; k++) {
		EXPECT_EQ(value_at(map, k), 0u);
	}
	EXPECT_FALSE(map.find(13));
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, InsertShiftsSubsequentKeys)
{
	SomethingMap map;
	for (uint32_t i = 0; i < 10; i++) map.update(0xffffffffu, i + 100); // [100..109]
	map.insert(5, 999);
	EXPECT_EQ(value_at(map, 4), 104u);
	EXPECT_EQ(value_at(map, 5), 999u);
	EXPECT_EQ(value_at(map, 6), 105u); // 元の5の値が6へ
	EXPECT_EQ(value_at(map, 10), 109u); // 末尾もずれる
	EXPECT_FALSE(map.find(11));
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, UpdateOverwritesWithoutShift)
{
	SomethingMap map;
	for (uint32_t i = 0; i < 10; i++) map.update(0xffffffffu, i + 100);
	map.update(5, 999);
	EXPECT_EQ(value_at(map, 5), 999u);
	EXPECT_EQ(value_at(map, 4), 104u);
	EXPECT_EQ(value_at(map, 6), 106u); // シフトしない
	EXPECT_FALSE(map.find(10));
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, UpdateBeyondMaxAppends)
{
	// 最大キーを超えたupdateは隙間を作らず末尾に追加する
	SomethingMap map;
	map.update(100, 1); // 空なのでkey 0に入る
	EXPECT_EQ(value_at(map, 0), 1u);
	EXPECT_FALSE(map.find(1));
	map.update(100, 2); // size=1を超えるのでkey 1に入る
	EXPECT_EQ(value_at(map, 1), 2u);
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, EraseShiftsSubsequentKeys)
{
	SomethingMap map;
	for (uint32_t i = 0; i < 10; i++) map.update(0xffffffffu, i + 100);
	map.erase(5);
	EXPECT_EQ(value_at(map, 4), 104u);
	EXPECT_EQ(value_at(map, 5), 106u); // 詰まる
	EXPECT_EQ(value_at(map, 8), 109u);
	EXPECT_FALSE(map.find(9));
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, EraseOutOfRangeIsNoop)
{
	SomethingMap map;
	map.update(0xffffffffu, 7);
	map.erase(100);
	EXPECT_EQ(value_at(map, 0), 7u);
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, EraseAllRemovesEverything)
{
	SomethingMap map;
	for (uint32_t i = 0; i < 100; i++) map.update(0xffffffffu, 1);
	for (uint32_t i = 0; i < 100; i++) map.erase(0);
	EXPECT_FALSE(map.find(0));
	EXPECT_EQ(map.count(100), 0u);
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, CountPrefixSums)
{
	// 折り返し数 [1,3,1,2] → 論理行iの先頭表示行はcount(i)
	SomethingMap map;
	uint32_t values[] = {1, 3, 1, 2};
	for (uint32_t v : values) map.update(0xffffffffu, v);
	EXPECT_EQ(map.count(0), 0u);
	EXPECT_EQ(map.count(1), 1u);
	EXPECT_EQ(map.count(2), 4u);
	EXPECT_EQ(map.count(3), 5u);
	EXPECT_EQ(map.count(4), 7u); // 総表示行数
	EXPECT_EQ(map.count(100), 7u); // 範囲外は全体の合計
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, CountAfterUpdateInsertErase)
{
	SomethingMap map;
	for (uint32_t i = 0; i < 4; i++) map.update(0xffffffffu, 1); // [1,1,1,1]
	map.update(1, 5); // [1,5,1,1]
	EXPECT_EQ(map.count(4), 8u);
	map.insert(2, 10); // [1,5,10,1,1]
	EXPECT_EQ(map.count(5), 18u);
	map.erase(1); // [1,10,1,1]
	EXPECT_EQ(map.count(4), 13u);
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, LeafSplit)
{
	// 葉の容量(256)を超える追加で分割が起き、全キーが正しく読める
	SomethingMap map;
	const uint32_t n = 300;
	for (uint32_t i = 0; i < n; i++) map.update(0xffffffffu, i % 4 + 1);
	EXPECT_TRUE(map.validate());
	uint64_t sum = 0;
	for (uint32_t i = 0; i < n; i++) {
		EXPECT_EQ(map.count(i), sum);
		ASSERT_EQ(value_at(map, i), i % 4 + 1);
		sum += i % 4 + 1;
	}
	// 満杯の葉の中間へのinsertでも分割して整合を保つ
	map.insert(100, 7);
	EXPECT_EQ(value_at(map, 100), 7u);
	EXPECT_EQ(value_at(map, 101), 100u % 4 + 1);
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, NodeSplit)
{
	// Nodeの容量(256葉 = 65536要素)を超えてNode分割が起きる規模
	SomethingMap map;
	const uint32_t n = 70000;
	uint64_t total = 0;
	for (uint32_t i = 0; i < n; i++) {
		uint32_t v = i % 3 + 1;
		map.update(0xffffffffu, v);
		total += v;
	}
	ASSERT_TRUE(map.validate());
	EXPECT_EQ(map.count(n), total);
	EXPECT_EQ(value_at(map, 0), 1u);
	EXPECT_EQ(value_at(map, n - 1), (n - 1) % 3 + 1);
	// 中間挿入で満杯Nodeの連鎖分割を起こす
	for (int i = 0; i < 1000; i++) map.insert(12345, 2);
	ASSERT_TRUE(map.validate());
	EXPECT_EQ(map.count(n + 1000), total + 2000u);
	EXPECT_FALSE(map.find(n + 1000));
}

TEST(SomethingMap, RandomOpsAgainstOracle)
{
	// std::vector<uint32_t> を正とするランダム操作比較
	SomethingMap map;
	std::vector<uint32_t> model;
	std::mt19937 rng(12345);
	for (int t = 0; t < 10000; t++) {
		uint32_t op = rng() % 3;
		uint32_t val = rng() % 4 + 1;
		if (op == 0) {
			uint32_t key = rng() % (model.size() + 20);
			map.insert(key, val);
			if (key > model.size()) model.resize(key, 0);
			model.insert(model.begin() + key, val);
		} else if (op == 1) {
			uint32_t key = rng() % (model.size() + 10);
			map.update(key, val);
			if (key < model.size()) model[key] = val;
			else model.push_back(val);
		} else {
			uint32_t key = rng() % (model.size() + 5);
			map.erase(key);
			if (key < model.size()) model.erase(model.begin() + key);
		}
	}
	ASSERT_TRUE(map.validate());
	uint64_t sum = 0;
	for (uint32_t k = 0; k < model.size(); k++) {
		ASSERT_EQ(value_at(map, k), model[k]) << "key=" << k;
		ASSERT_EQ(map.count(k), sum) << "key=" << k;
		sum += model[k];
	}
	EXPECT_FALSE(map.find(model.size()));
	EXPECT_EQ(map.count(model.size()), sum);
}

typedef std::pair<uint32_t, uint32_t> VL; // (論理行, 行内オフセット)

static inline std::pair<uint32_t, uint32_t> LP(SomethingMap::LogicalPosition const &lp)
{
	std::pair<uint32_t, uint32_t> ret = {lp.lrow, lp.wrap_index};
	return ret;
}

TEST(SomethingMap, VisualToLogicalBasics)
{
	// 折り返し数 [1,3,1,2] → 表示行は0..6の7行
	SomethingMap map;
	uint32_t values[] = {1, 3, 1, 2};
	for (uint32_t v : values) map.update(0xffffffffu, v);
	EXPECT_EQ(LP(map.visual_to_logical(0)), (VL{0, 0}));
	EXPECT_EQ(LP(map.visual_to_logical(1)), (VL{1, 0}));
	EXPECT_EQ(LP(map.visual_to_logical(2)), (VL{1, 1}));
	EXPECT_EQ(LP(map.visual_to_logical(3)), (VL{1, 2}));
	EXPECT_EQ(LP(map.visual_to_logical(4)), (VL{2, 0}));
	EXPECT_EQ(LP(map.visual_to_logical(5)), (VL{3, 0}));
	EXPECT_EQ(LP(map.visual_to_logical(6)), (VL{3, 1}));
	EXPECT_EQ(map.visual_to_logical(7).lrow, 4u); // 総表示行数以上は末尾の次(=総論理行数)
	EXPECT_EQ(map.visual_to_logical(100).lrow, 4u);
	EXPECT_EQ(SomethingMap().visual_to_logical(0).lrow, 0u); // 空マップ(総論理行数=0)
}

TEST(SomethingMap, VisualToLogicalSkipsZeroValueItems)
{
	// value 0 の行(隙間0埋め)は表示行を持たないのでスキップされる
	SomethingMap map;
	map.insert(2, 5); // [0,0,5]
	EXPECT_EQ(LP(map.visual_to_logical(0)), (VL{2, 0}));
	EXPECT_EQ(LP(map.visual_to_logical(4)), (VL{2, 4}));
	EXPECT_EQ(map.visual_to_logical(5).lrow, 3u); // 範囲外は末尾の次
}

TEST(SomethingMap, VisualToLogicalIsInverseOfCount)
{
	// ランダム構築(value 0含む)後、全表示行についてcountとの整合を確認
	SomethingMap map;
	std::vector<uint32_t> model;
	std::mt19937 rng(777);
	for (int i = 0; i < 600; i++) {
		uint32_t v = rng() % 4; // 0..3(value 0も混ぜる)
		map.update(0xffffffffu, v);
		model.push_back(v);
	}
	ASSERT_TRUE(map.validate());
	uint64_t r = 0;
	for (uint32_t i = 0; i < model.size(); i++) {
		for (uint32_t off = 0; off < model[i]; off++) {
			ASSERT_EQ(LP(map.visual_to_logical(r)), (VL{i, off})) << "visual_row=" << r;
			r++;
		}
	}
	EXPECT_EQ(map.count(model.size()), r); // 総表示行数
	EXPECT_EQ(map.visual_to_logical(r).lrow, (uint32_t)model.size()); // 範囲外は末尾の次
	// 各論理行の先頭表示行との往復: value>0 の行は visual_to_logical(count(i)) == (i, 0)
	for (uint32_t i = 0; i < model.size(); i++) {
		if (model[i] > 0) {
			ASSERT_EQ(LP(map.visual_to_logical(map.count(i))), (VL{i, 0})) << "logical=" << i;
		}
	}
}

TEST(SomethingMap, VisualToLogicalLargeWithNodeSplit)
{
	// Node分割が起きる規模(7万行)での往復整合
	SomethingMap map;
	const uint32_t n = 70000;
	for (uint32_t i = 0; i < n; i++) map.update(0xffffffffu, i % 3 + 1);
	ASSERT_TRUE(map.validate());
	for (uint32_t i = 0; i < n; i += 97) {
		uint64_t head = map.count(i);
		ASSERT_EQ(LP(map.visual_to_logical(head)), (VL{i, 0})) << "logical=" << i;
		uint32_t v = i % 3 + 1;
		ASSERT_EQ(LP(map.visual_to_logical(head + v - 1)), (VL{i, v - 1})) << "logical=" << i;
	}
	uint64_t total = map.count(n);
	EXPECT_EQ(LP(map.visual_to_logical(total - 1)), (VL{n - 1, (n - 1) % 3}));
	EXPECT_EQ(map.visual_to_logical(total).lrow, n); // 範囲外は末尾の次
}

TEST(SomethingMap, LocateColumn)
{
	// 折り返し行の桁数 [5,3,2]: 列0..4→行0、列5..7→行1、列8..→行2(最終行は切らない)
	SomethingMap::ValueItem item(std::vector<uint32_t>{5, 3, 2});
	EXPECT_EQ(item.value(), 3u);
	typedef std::pair<uint32_t, uint32_t> RC; // (折り返し行, 行内列)
	EXPECT_EQ(item.locate_column(0), (RC{0, 0}));
	EXPECT_EQ(item.locate_column(4), (RC{0, 4}));
	EXPECT_EQ(item.locate_column(5), (RC{1, 0})); // 桁数ちょうどの境界は次の行頭へ
	EXPECT_EQ(item.locate_column(7), (RC{1, 2}));
	EXPECT_EQ(item.locate_column(8), (RC{2, 0}));
	EXPECT_EQ(item.locate_column(99), (RC{2, 91})); // 行末超過は最終行に丸める
	// col_len未設定(0)の行は折り返し境界として扱われない
	SomethingMap::ValueItem plain(3);
	EXPECT_EQ(plain.locate_column(0), (RC{0, 0}));
	EXPECT_EQ(plain.locate_column(99), (RC{0, 99}));
	// column_of_row は locate_column の逆演算(折り返し行の先頭論理列)
	EXPECT_EQ(item.column_of_row(0), 0u);
	EXPECT_EQ(item.column_of_row(1), 5u);
	EXPECT_EQ(item.column_of_row(2), 8u);
}

TEST(SomethingMap, LogicalToVisual)
{
	// 行0: 1表示行(4桁)、行1: 3表示行(5,3,2桁)、行2: 1表示行(4桁)
	SomethingMap map;
	map.update(0xffffffffu, std::vector<uint32_t>{4});
	map.update(0xffffffffu, std::vector<uint32_t>{5, 3, 2});
	map.update(0xffffffffu, std::vector<uint32_t>{4});
	ASSERT_TRUE(map.validate());
	// 行0は折り返しなし: どの列でも表示行0
	EXPECT_EQ(map.logical_to_visual(0, 0).vrow, 0u);
	EXPECT_EQ(map.logical_to_visual(0, 99).vrow, 0u);
	// 行1(先頭表示行=1): 列に応じて折り返し行が進み、vcolは折り返し行内の列になる
	EXPECT_EQ(map.logical_to_visual(1, 0).vrow, 1u);
	EXPECT_EQ(map.logical_to_visual(1, 4).vrow, 1u);
	EXPECT_EQ(map.logical_to_visual(1, 4).vcol, 4u);
	EXPECT_EQ(map.logical_to_visual(1, 5).vrow, 2u);
	EXPECT_EQ(map.logical_to_visual(1, 5).vcol, 0u); // 桁数ちょうどの境界は次の行頭
	EXPECT_EQ(map.logical_to_visual(1, 7).vrow, 2u);
	EXPECT_EQ(map.logical_to_visual(1, 7).vcol, 2u);
	EXPECT_EQ(map.logical_to_visual(1, 8).vrow, 3u);
	EXPECT_EQ(map.logical_to_visual(1, 99).vrow, 3u);
	EXPECT_EQ(map.logical_to_visual(1, 99).vcol, 91u); // 行末超過は最終行に丸める
	// 逆変換のlcol: 折り返し行の先頭の論理列が返る
	EXPECT_EQ(map.visual_to_logical(1).lcol, 0u);
	EXPECT_EQ(map.visual_to_logical(2).lcol, 5u);
	EXPECT_EQ(map.visual_to_logical(3).lcol, 8u);
	EXPECT_EQ(map.visual_to_logical(4).lcol, 0u); // 次の論理行の先頭
	// 行2(先頭表示行=4)
	EXPECT_EQ(map.logical_to_visual(2, 0).vrow, 4u);
	// 論理行が範囲外なら総表示行数
	EXPECT_EQ(map.logical_to_visual(3, 0).vrow, 5u);
	EXPECT_EQ(map.logical_to_visual(100, 0).vrow, 5u);
	EXPECT_EQ(map.logical_to_visual(100, 0).vcol, 0u);
	// 逆変換との往復: 列0なら (行, 折り返し0) に戻る
	for (uint32_t i = 0; i < 3; i++) {
		EXPECT_EQ(LP(map.visual_to_logical(map.logical_to_visual(i, 0).vrow)), (VL{i, 0}));
	}
	// col_len未設定の行は列によらず行頭の表示行を返す
	map.update(1, 3);
	EXPECT_EQ(map.logical_to_visual(1, 99).vrow, 1u);
	ASSERT_TRUE(map.validate());
}

TEST(SomethingMap, Clear)
{
	SomethingMap map;
	for (uint32_t i = 0; i < 1000; i++) map.update(0xffffffffu, 2);
	map.clear();
	EXPECT_FALSE(map.find(0));
	EXPECT_EQ(map.count(1000), 0u);
	EXPECT_EQ(map.visual_to_logical(0).lrow, 0u); // 空 = 総論理行数0
	EXPECT_EQ(map.total_logical_row_count(), 0u);
	EXPECT_EQ(map.total_visual_row_count(), 0u);
	EXPECT_TRUE(map.validate());
	// clear後も再構築できる
	map.update(0xffffffffu, 5);
	EXPECT_EQ(value_at(map, 0), 5u);
	EXPECT_TRUE(map.validate());
}

TEST(SomethingMap, TotalRowCounts)
{
	SomethingMap map;
	EXPECT_EQ(map.total_logical_row_count(), 0u);
	EXPECT_EQ(map.total_visual_row_count(), 0u);
	uint32_t values[] = {1, 3, 0, 2}; // value 0 の行も論理行としては数える
	for (uint32_t v : values) map.update(0xffffffffu, v);
	EXPECT_EQ(map.total_logical_row_count(), 4u);
	EXPECT_EQ(map.total_visual_row_count(), 6u);
	EXPECT_EQ(map.count(4), map.total_visual_row_count()); // count(総論理行数) と一致
	// Node分割が起きる規模でも一致する
	for (uint32_t i = 0; i < 70000; i++) map.update(0xffffffffu, 2);
	ASSERT_TRUE(map.validate());
	EXPECT_EQ(map.total_logical_row_count(), 70004u);
	EXPECT_EQ(map.total_visual_row_count(), 6u + 140000u);
	// 範囲外の visual_to_logical は総論理行数を返す(末尾の次)
	EXPECT_EQ(map.visual_to_logical(map.total_visual_row_count()).lrow, map.total_logical_row_count());
}
