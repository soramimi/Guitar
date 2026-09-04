#ifndef SOMETHINGMAP_H
#define SOMETHINGMAP_H

#include <cstdint>
#include <vector>
#include <optional>
#include <memory>
#include <utility>
#include <assert.h>
#include <limits>

// SomethingMap
//
// 折り返し(ワードラップ)機能付きテキストエディタで、論理行番号と物理行番号を
// 相互変換するためのデータ構造。
//
// - キー = 論理行番号(0ベース)。安定したIDではなく「位置」であり、
//   insert で後続キーは1つ後ろへずれ、erase で1つ前へ詰まる
// - 値 = その論理行が折り返しで占める物理行数。物理行1つにつき UserData を
//   1要素持ち、その個数(value())が折り返し数を兼ねる。UserData の col_len に
//   各折り返し行の桁数を持たせると、論理列まで含めた座標変換ができる
// - count(i) = 論理行 0..i-1 の物理行数の合計 = 論理行 i の先頭の物理行番号
// - logical_to_visual(行, 列) = 順変換(論理座標 → 物理行番号)
// - visual_to_logical(物理行) = 逆変換(物理行番号 → 論理行と行内オフセット)
//
// 内部は固定3階層の B+-tree: root(nodes_)→ Node → Leaf。
// Node に「配下の総要素数」と「配下の値の合計」をキャッシュしておき、
// キー解決・集計・逆変換のいずれも Node/Leaf 単位のスキップで高速化する。
// 各操作の計算量は O(root内Node数 + fanout + 葉容量)。
class SomethingMap {
public:
	// 物理行(折り返し行)1行ぶんに付随するユーザーデータ
	struct UserData {
		uint32_t vcol_len = 0; // この折り返し行が保持する桁数(0 = 未設定)
	};
	// 論理行1行ぶんの値。物理行ごとの UserData の配列を共有ポインタで保持し、
	// その要素数が折り返し数(=この行が占める物理行数)を表す。
	// コピーは shared_ptr の浅い共有なのでコピーコストは小さい。
	// data_ は private のため外部からは変更できず、折り返し数の変更は
	// 必ず SomethingMap::update() を通る(sum_values との整合が保たれる)。
	class ValueItem {
		friend class SomethingMap;
	private:
		std::shared_ptr<std::vector<UserData>> data_;
	public:
		// 折り返し数だけ指定して構築する(col_len はすべて未設定)
		ValueItem(uint32_t value = 0)
		{
			data_ = std::make_shared<std::vector<UserData>>(value);
		}
		// 各折り返し行の桁数を指定して構築する(折り返し数 = col_count_list.size())
		ValueItem(std::vector<uint32_t> const &col_count_list)
		{
			data_ = std::make_shared<std::vector<UserData>>(col_count_list.size());
			for (size_t i = 0; i < col_count_list.size(); i++) {
				(*data_)[i].vcol_len = col_count_list[i];
			}
		}
		// 折り返し数(この行が占める物理行数)
		uint32_t value() const
		{
			assert(data_);
			return data_->size();
		}
		// 論理列がこの行の何番目の折り返し行に属するかを求める。
		// 戻り値は (折り返し行インデックス, 折り返し行内の列)。
		// 列 == 桁数 の境界は次の折り返し行の先頭に進む。
		// 最終折り返し行では列を切らないため、行末を超えた列は最終行に丸められる。
		// col_len が未設定(0)の行は折り返し境界として扱われない。
		std::pair<uint32_t, uint32_t> locate_column(uint32_t lcol) const
		{
			assert(data_);
			uint32_t row = 0;
			for (size_t i = 0; i + 1 < data_->size(); i++) {
				uint32_t len = (*data_)[i].vcol_len;
				if (len > 0) {
					if (lcol < len) break;
					lcol -= len;
					row++;
				}
			}
			return {row, lcol};
		}
	};
	typedef uint32_t key_type;
	typedef ValueItem value_type;
private:
	static constexpr size_t max_leaf_capacity = 256; // Leafが保持できるitem数の上限
	static constexpr size_t max_node_fanout = 256;   // Nodeが保持できるLeaf数の上限
	// 第3階層。item(論理行)の実体を保持する。
	// 不変条件: sum_values は items の value() の総和と常に一致し、
	// サイズは 1..max_leaf_capacity(空のLeafは残さない)。
	struct Leaf {
		std::vector<value_type> items;
		uint64_t sum_values = 0;
	};
	// 第2階層。Leafの列と、配下全体の集計値を保持する。
	// num_items はキー解決の、sum_values は count/逆変換のスキップに使う。
	// 不変条件: 集計値は配下のLeafの合計と常に一致し、空のNodeは残さない。
	struct Node {
		std::vector<Leaf> leaves;
		size_t num_items = 0;    // 配下の総item数
		uint64_t sum_values = 0; // 配下のvalue()の総和
	};
	// 第1階層(root)。キーは先頭のNodeから順に num_items を差し引いて解決する
	// 通算インデックス。
	std::vector<Node> nodes_;
private:
	// 末尾に追記可能な(満杯でない)Leafを用意する。
	// 最後のLeafが満杯なら新しいLeafを、最後のNodeのfanoutも満杯なら
	// 新しいNodeを追加する。末尾への連続追加はこの経路で分割を起こさずに
	// 満杯まで詰めて構築されるため、一括構築が速い。
	void ensure_tail()
	{
		if (nodes_.empty()) {
			nodes_.push_back(Node());
		}
		Node *node = &nodes_.back();
		if (node->leaves.empty() || node->leaves.back().items.size() >= max_leaf_capacity) {
			if (node->leaves.size() >= max_node_fanout) {
				nodes_.push_back(Node());
				node = &nodes_.back();
			}
			Leaf leaf;
			leaf.items.reserve(max_leaf_capacity);
			node->leaves.push_back(std::move(leaf));
		}
	}
	// Nodeの集計値(num_items / sum_values)を配下のLeafから再計算する
	void recalc_node(Node *node)
	{
		node->num_items = 0;
		node->sum_values = 0;
		for (Leaf const &leaf : node->leaves) {
			node->num_items += leaf.items.size();
			node->sum_values += leaf.sum_values;
		}
	}
	// nodes_[ni] のLeaf列を半分に分けて、後半を新しいNodeとして直後に挿入する
	void split_node(size_t ni)
	{
		Node *node = &nodes_[ni];
		size_t half = node->leaves.size() / 2;
		Node newnode;
		newnode.leaves.assign(std::make_move_iterator(node->leaves.begin() + half), std::make_move_iterator(node->leaves.end()));
		node->leaves.resize(half);
		recalc_node(node);
		recalc_node(&newnode);
		nodes_.insert(nodes_.begin() + ni + 1, std::move(newnode));
	}
	// nodes_[ni] の li 番目のLeafの offset 位置に item を挿入する。
	// 挿入・末尾追加の変更はすべてここを通ることで、集計値の更新漏れを防ぐ。
	// Leafが容量を超えたら半分に分割し、その結果Nodeのfanoutを超えたら
	// Nodeも分割する(分割の連鎖はここで完結する)。
	void insert_into_leaf(size_t ni, size_t li, size_t offset, value_type item)
	{
		Node *node = &nodes_[ni];
		Leaf *leaf = &node->leaves[li];
		leaf->items.insert(leaf->items.begin() + offset, item);
		leaf->sum_values += item.value();
		node->num_items++;
		node->sum_values += item.value();
		if (leaf->items.size() > max_leaf_capacity) {
			// 後半を新しいLeafへ移し、集計値も付け替える
			size_t half = leaf->items.size() / 2;
			Leaf newleaf;
			newleaf.items.reserve(max_leaf_capacity);
			newleaf.items.assign(leaf->items.begin() + half, leaf->items.end());
			for (value_type const &x : newleaf.items) {
				newleaf.sum_values += x.value();
			}
			leaf->sum_values -= newleaf.sum_values;
			leaf->items.resize(half);
			node->leaves.insert(node->leaves.begin() + li + 1, std::move(newleaf));
			if (node->leaves.size() > max_node_fanout) {
				split_node(ni);
			}
		}
	}
	// count() の走査結果。前置和と、key位置のitemを同時に返すことで、
	// logical_to_visual での二度引きを避ける。
	struct CountResult {
		uint64_t sum = 0;                // [0, key) の value() の合計
		ValueItem const *item = nullptr; // key位置のitem(範囲外なら nullptr)
	};
	// [0, lrow) の値の合計と、key位置のitemを求める。
	// Node/Leaf 全体が範囲に収まる場合は集計値を一括加算してスキップし、
	// 境界がかかる最後のLeafだけ個別に加算する。
	CountResult count_and_find(key_type lrow) const
	{
		CountResult result;
		for (Node const &node : nodes_) {
			// Node全体が範囲に収まるなら集計値を一括加算してスキップ
			if (lrow >= node.num_items) {
				result.sum += node.sum_values;
				lrow -= node.num_items;
				if (lrow == 0) break;
				continue;
			}
			for (Leaf const &leaf : node.leaves) {
				size_t nvalues = leaf.items.size();
				if (lrow < nvalues) {
					// 境界がかかる最後のLeafだけ個別に加算する
					for (size_t j = 0; j < lrow; j++) {
						result.sum += leaf.items[j].value();
					}
					result.item = &leaf.items[lrow];
					break;
				}
				// Leaf全体が範囲に収まるなら集計値を一括加算してスキップ
				result.sum += leaf.sum_values;
				lrow -= nvalues;
				if (lrow == 0) break;
			}
			break;
		}
		return result;
	}
public:
	// 全不変条件を検査する(テスト用)。
	// 集計値の一致・容量とfanoutの上限・空のLeaf/Nodeが残っていないことを確認し、
	// すべて整合していれば true を返す。
	bool validate() const
	{
		for (Node const &node : nodes_) {
			if (node.leaves.empty()) return false;
			if (node.leaves.size() > max_node_fanout) return false;
			size_t num_items = 0;
			uint64_t node_sum = 0;
			for (Leaf const &leaf : node.leaves) {
				if (leaf.items.empty()) return false;
				if (leaf.items.size() > max_leaf_capacity) return false;
				uint64_t sum = 0;
				for (value_type const &x : leaf.items) {
					sum += x.value();
				}
				if (leaf.sum_values != sum) return false;
				num_items += leaf.items.size();
				node_sum += sum;
			}
			if (node.num_items != num_items) return false;
			if (node.sum_values != node_sum) return false;
		}
		return true;
	}
	// すべて消去する(画面幅変更時の全再構築などに使う)
	void clear()
	{
		nodes_.clear();
	}
	// lrow 位置の値を返す。範囲外は nullopt。
	std::optional<value_type> find(key_type lrow) const
	{
		for (Node const &node : nodes_) {
			// このNodeの範囲外ならNodeごとスキップ
			if (lrow >= node.num_items) {
				lrow -= node.num_items;
				continue;
			}
			for (Leaf const &leaf : node.leaves) {
				size_t nvalues = leaf.items.size();
				if (lrow < nvalues) {
					return leaf.items[lrow];
				}
				lrow -= nvalues;
			}
			break; // Nodeに入ったら必ずLeaf内で解決する(ここには到達しない)
		}
		return std::nullopt;
	}
	// [0, lrow) の範囲の値の合計を返す。
	// 論理行 lrow の先頭の物理行番号に相当する。lrow が総要素数を超えていたら
	// 全体の合計(=総物理行数)を返す。
	uint64_t count(key_type lrow) const
	{
		return count_and_find(lrow).sum;
	}
	// lrow の位置に item を挿入する。後続のキーは1つ後ろへずれる。
	// lrow が末尾より先の場合は、隙間をデフォルト値(value 0)で埋めてから配置する。
	void insert(key_type lrow, value_type item)
	{
		for (size_t ni = 0; ni < nodes_.size(); ni++) {
			Node const &node = nodes_[ni];
			// 挿入は末尾境界(lrow == num_items)もこのNodeが受け持つ
			if (lrow > node.num_items) {
				lrow -= node.num_items;
				continue;
			}
			for (size_t li = 0; li < node.leaves.size(); li++) {
				size_t nvalues = node.leaves[li].items.size();
				if (lrow <= nvalues) {
					insert_into_leaf(ni, li, lrow, item);
					return;
				}
				lrow -= nvalues;
			}
			return; // Nodeに入ったら必ずLeaf内で解決する(ここには到達しない)
		}
		// 末尾より先: 隙間をデフォルト値(value 0)で埋めてから追加する
		while (lrow > 0) {
			ensure_tail();
			Node *node = &nodes_.back();
			std::vector<value_type> *items = &node->leaves.back().items;
			size_t n = std::min<size_t>(lrow, max_leaf_capacity - items->size());
			items->insert(items->end(), n, {});
			node->num_items += n; // value 0 なので sum_values は不変
			lrow -= n;
		}
		ensure_tail();
		insert_into_leaf(nodes_.size() - 1, nodes_.back().leaves.size() - 1, nodes_.back().leaves.back().items.size(), item);
	}
	// 既存の lrow なら値を上書きする(後続のキーはずれない)。
	// lrow が最大キーを超えていたら末尾に追加する(隙間埋めはしない)。
	// 末尾追加の性質を使って、連続appendによる一括構築にも使える。
	void update(key_type lrow, value_type value)
	{
		for (Node &node : nodes_) {
			// このNodeの範囲外ならNodeごとスキップ
			if (lrow >= node.num_items) {
				lrow -= node.num_items;
				continue;
			}
			for (Leaf &leaf : node.leaves) {
				size_t nvalues = leaf.items.size();
				if (lrow < nvalues) {
					// 古い値を差し引いてから新しい値を加算し、上書きする
					uint64_t oldvalue = leaf.items[lrow].value();
					uint64_t newvalue = value.value();
					leaf.sum_values -= oldvalue;
					leaf.sum_values += newvalue;
					node.sum_values -= oldvalue;
					node.sum_values += newvalue;
					leaf.items[lrow] = value;
					return;
				}
				lrow -= nvalues;
			}
			return; // Nodeに入ったら必ずLeaf内で解決する(ここには到達しない)
		}
		// 最大キーを超えていたら末尾に追加する
		ensure_tail();
		insert_into_leaf(nodes_.size() - 1, nodes_.back().leaves.size() - 1, nodes_.back().leaves.back().items.size(), value);
	}
	// lrow 位置の要素を削除する。後続のキーは1つ前へ詰まる。範囲外なら何もしない。
	// 空になったLeafはNodeから、空になったNodeはrootから取り除く。
	void erase(key_type lrow)
	{
		for (size_t ni = 0; ni < nodes_.size(); ni++) {
			Node *node = &nodes_[ni];
			// このNodeの範囲外ならNodeごとスキップ
			if (lrow >= node->num_items) {
				lrow -= node->num_items;
				continue;
			}
			for (size_t li = 0; li < node->leaves.size(); li++) {
				Leaf *leaf = &node->leaves[li];
				size_t nvalues = leaf->items.size();
				if (lrow < nvalues) {
					uint64_t oldvalue = leaf->items[lrow].value();
					leaf->sum_values -= oldvalue;
					node->sum_values -= oldvalue;
					node->num_items--;
					leaf->items.erase(leaf->items.begin() + lrow);
					if (leaf->items.empty()) {
						node->leaves.erase(node->leaves.begin() + li);
						if (node->leaves.empty()) {
							nodes_.erase(nodes_.begin() + ni);
						}
					}
					return;
				}
				lrow -= nvalues;
			}
			return; // Nodeに入ったら必ずLeaf内で解決する(ここには到達しない)
		}
	}
	
	// (論理行, 論理列) から物理行番号を求める。countの順変換。
	// 行頭の物理行(count)に、論理列が属する折り返し行のオフセット
	// (ValueItem::locate_column)を加える。論理行が範囲外なら総物理行数を返す。
	struct VisualPosition {
		uint64_t vrow = 0; // 物理行番号
		uint32_t vcol = 0; // 行内の折り返し行オフセット
	};
	VisualPosition logical_to_visual(key_type lrow, uint32_t lcol) const
	{
		CountResult r = count_and_find(lrow);
		if (r.item) {
			auto [row, col] = r.item->locate_column(lcol);
			auto vrow = r.sum + row;
			auto vcol = col;
			if (vrow <= std::numeric_limits<uint32_t>::max()) {
				VisualPosition ret;
				ret.vrow = vrow;
				ret.vcol = vcol;
				return ret;
			}
		}
		return {r.sum, 0};
	}
	// 物理行番号から (論理行番号, 行内の折り返し行オフセット) を求める。countの逆変換。
	// count(i) <= vrow < count(i+1) となる論理行 i を返す。
	// value 0 の行は物理行を持たないためスキップされる。
	struct LogicalPosition {
		uint32_t lrow = 0; // 論理行番号
		uint32_t lcol = 0; // 論理列番号
		uint32_t vrow_in_the_lrow = 0; // 行内の折り返し行オフセット
	};
	LogicalPosition visual_to_logical(uint64_t vrow) const
	{
		key_type lrow = 0;
		for (Node const &node : nodes_) {
			// このNodeの物理行範囲より先ならNodeごとスキップ
			if (vrow >= node.sum_values) {
				vrow -= node.sum_values;
				lrow += node.num_items;
				continue;
			}
			for (Leaf const &leaf : node.leaves) {
				// このLeafの物理行範囲より先ならLeafごとスキップ
				if (vrow >= leaf.sum_values) {
					vrow -= leaf.sum_values;
					lrow += leaf.items.size();
					continue;
				}
				// 該当するLeafの中を個別に引いていく
				for (value_type const &item : leaf.items) {
					uint32_t v = item.value(); // この論理行が占める物理行数
					if (vrow < v) {
						LogicalPosition ret;
						ret.vrow_in_the_lrow = vrow;
						ret.lrow = lrow;
						ret.lcol = 0;
						if (item.data_) {
							for (size_t i = 0; i < vrow; i++) {
								ret.lcol += (*item.data_)[i].vcol_len;
							}
						}
						return ret;
					}
					vrow -= v;
					lrow++;
				}
			}
		}
		return {};
	}
	//
	uint64_t total_logical_row_count() const
	{
		uint64_t total = 0;
		for (Node const &node : nodes_) {
			total += node.num_items;
		}
		return total;
	}
	//
	uint64_t total_visual_row_count() const
	{
		uint64_t total = 0;
		for (Node const &node : nodes_) {
			total += node.sum_values;
		}
		return total;
	}
};

#endif // SOMETHINGMAP_H
