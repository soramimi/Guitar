#ifndef SOMETHINGMAP_H
#define SOMETHINGMAP_H

#include <cstdint>
#include <vector>
#include <optional>
#include <memory>
#include <utility>
#include <assert.h>

// SomethingMap
//
// 折り返し(ワードラップ)機能付きテキストエディタで、論理行番号と表示行番号を
// 相互変換するためのデータ構造。
//
// - キー = 論理行番号(0ベース)。安定したIDではなく「位置」であり、
//   insert で後続キーは1つ後ろへずれ、erase で1つ前へ詰まる
// - 値 = その論理行が折り返しで占める表示行数。表示行1つにつき UserData を
//   1要素持ち、その個数(value())が折り返し数を兼ねる
// - count(i) = 論理行 0..i-1 の表示行数の合計 = 論理行 i の先頭の表示行番号
// - visual_to_logical(r) = count の逆変換
//
// 内部は固定3階層の B+-tree: root(nodes_)→ Node → Leaf。
// Node に「配下の総要素数」と「配下の値の合計」をキャッシュしておき、
// キー解決・集計・逆変換のいずれも Node/Leaf 単位のスキップで高速化する。
// 各操作の計算量は O(root内Node数 + fanout + 葉容量)。
class SomethingMap {
public:
	// 表示行1行ぶんに付随するユーザーデータ(折り返し位置などを載せる想定)
	struct UserData {
	};
	// 論理行1行ぶんの値。表示行ごとの UserData の配列を共有ポインタで保持し、
	// その要素数が折り返し数(=この行が占める表示行数)を表す。
	class ValueItem {
	private:
		std::shared_ptr<std::vector<UserData>> data_;
	public:
		void _init(uint32_t value)
		{
			data_ = std::make_shared<std::vector<UserData>>(value);
		}
		ValueItem()
		{
			_init(0);
		}
		ValueItem(uint32_t value)
		{
			_init(value);
		}
		// 折り返し数(この行が占める表示行数)
		uint32_t value() const
		{
			assert(data_);
			return data_->size();
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
	// すべて消去
	void clear()
	{
		nodes_.clear();
	}
	// key 位置の値を返す。範囲外は nullopt。
	std::optional<value_type> find(key_type key) const
	{
		for (Node const &node : nodes_) {
			// このNodeの範囲外ならNodeごとスキップ
			if (key >= node.num_items) {
				key -= node.num_items;
				continue;
			}
			for (Leaf const &leaf : node.leaves) {
				size_t nvalues = leaf.items.size();
				if (key < nvalues) {
					return leaf.items[key];
				}
				key -= nvalues;
			}
			break; // Nodeに入ったら必ずLeaf内で解決する(ここには到達しない)
		}
		return std::nullopt;
	}
	// [0, key) の範囲の値の合計を返す。
	// 論理行 key の先頭の表示行番号に相当する。key が総要素数を超えていたら
	// 全体の合計(=総表示行数)を返す。
	std::pair<uint64_t, ValueItem const *> count(key_type key) const
	{
		ValueItem const *item = nullptr;
		uint64_t sum = 0;
		for (Node const &node : nodes_) {
			// Node全体が範囲に収まるなら集計値を一括加算してスキップ
			if (key >= node.num_items) {
				sum += node.sum_values;
				key -= node.num_items;
				if (key == 0) break;
				continue;
			}
			for (Leaf const &leaf : node.leaves) {
				size_t nvalues = leaf.items.size();
				if (key >= nvalues) {
					// Leaf全体が範囲に収まるなら集計値を一括加算してスキップ
					sum += leaf.sum_values;
					key -= nvalues;
					if (key == 0) break;
				} else {
					// 境界がかかる最後のLeafだけ個別に加算する
					for (size_t j = 0; j < key; j++) {
						item = &leaf.items[j];
						sum += leaf.items[j].value();
					}
					break;
				}
			}
			break;
		}
		return {sum, item};
	}
	// key の位置に item を挿入する。後続のキーは1つ後ろへずれる。
	// key が末尾より先の場合は、隙間をデフォルト値(value 0)で埋めてから配置する。
	void insert(key_type key, value_type item)
	{
		for (size_t ni = 0; ni < nodes_.size(); ni++) {
			Node const &node = nodes_[ni];
			// 挿入は末尾境界(key == num_items)もこのNodeが受け持つ
			if (key > node.num_items) {
				key -= node.num_items;
				continue;
			}
			for (size_t li = 0; li < node.leaves.size(); li++) {
				size_t nvalues = node.leaves[li].items.size();
				if (key <= nvalues) {
					insert_into_leaf(ni, li, key, item);
					return;
				}
				key -= nvalues;
			}
			return; // Nodeに入ったら必ずLeaf内で解決する(ここには到達しない)
		}
		// 末尾より先: 隙間をデフォルト値(value 0)で埋めてから追加する
		while (key > 0) {
			ensure_tail();
			Node *node = &nodes_.back();
			std::vector<value_type> *items = &node->leaves.back().items;
			size_t n = std::min<size_t>(key, max_leaf_capacity - items->size());
			items->insert(items->end(), n, {});
			node->num_items += n; // value 0 なので sum_values は不変
			key -= n;
		}
		ensure_tail();
		insert_into_leaf(nodes_.size() - 1, nodes_.back().leaves.size() - 1, nodes_.back().leaves.back().items.size(), item);
	}
	// 既存の key なら値を上書きする(後続のキーはずれない)。
	// key が最大キーを超えていたら末尾に追加する(隙間埋めはしない)。
	// 末尾追加の性質を使って、連続appendによる一括構築にも使える。
	void update(key_type key, value_type value)
	{
		for (Node &node : nodes_) {
			// このNodeの範囲外ならNodeごとスキップ
			if (key >= node.num_items) {
				key -= node.num_items;
				continue;
			}
			for (Leaf &leaf : node.leaves) {
				size_t nvalues = leaf.items.size();
				if (key < nvalues) {
					// 古い値を差し引いてから新しい値を加算し、上書きする
					uint64_t oldvalue = leaf.items[key].value();
					uint64_t newvalue = value.value();
					leaf.sum_values -= oldvalue;
					leaf.sum_values += newvalue;
					node.sum_values -= oldvalue;
					node.sum_values += newvalue;
					leaf.items[key] = value;
					return;
				}
				key -= nvalues;
			}
			return; // Nodeに入ったら必ずLeaf内で解決する(ここには到達しない)
		}
		// 最大キーを超えていたら末尾に追加する
		ensure_tail();
		insert_into_leaf(nodes_.size() - 1, nodes_.back().leaves.size() - 1, nodes_.back().leaves.back().items.size(), value);
	}
	// key 位置の要素を削除する。後続のキーは1つ前へ詰まる。範囲外なら何もしない。
	// 空になったLeafはNodeから、空になったNodeはrootから取り除く。
	void erase(key_type key)
	{
		for (size_t ni = 0; ni < nodes_.size(); ni++) {
			Node *node = &nodes_[ni];
			// このNodeの範囲外ならNodeごとスキップ
			if (key >= node->num_items) {
				key -= node->num_items;
				continue;
			}
			for (size_t li = 0; li < node->leaves.size(); li++) {
				Leaf *leaf = &node->leaves[li];
				size_t nvalues = leaf->items.size();
				if (key < nvalues) {
					uint64_t oldvalue = leaf->items[key].value();
					leaf->sum_values -= oldvalue;
					node->sum_values -= oldvalue;
					node->num_items--;
					leaf->items.erase(leaf->items.begin() + key);
					if (leaf->items.empty()) {
						node->leaves.erase(node->leaves.begin() + li);
						if (node->leaves.empty()) {
							nodes_.erase(nodes_.begin() + ni);
						}
					}
					return;
				}
				key -= nvalues;
			}
			return; // Nodeに入ったら必ずLeaf内で解決する(ここには到達しない)
		}
	}
	// 論理行番号から表示行番号を求める。countの順変換。
	uint64_t logical_to_visual(uint64_t logical_row) const
	{
		return count(logical_row).first;
	}
	// 表示行番号から (論理行番号, 行内の折り返し行オフセット) を求める。countの逆変換。
	// count(i) <= visual_row < count(i+1) となる論理行 i を返す。
	// value 0 の行は表示行を持たないためスキップされる。範囲外は nullopt。
	std::optional<std::pair<key_type, key_type>> visual_to_logical(uint64_t visual_row) const
	{
		key_type logical = 0;
		for (Node const &node : nodes_) {
			// このNodeの表示行範囲より先ならNodeごとスキップ
			if (visual_row >= node.sum_values) {
				visual_row -= node.sum_values;
				logical += node.num_items;
				continue;
			}
			for (Leaf const &leaf : node.leaves) {
				// このLeafの表示行範囲より先ならLeafごとスキップ
				if (visual_row >= leaf.sum_values) {
					visual_row -= leaf.sum_values;
					logical += leaf.items.size();
					continue;
				}
				// 該当するLeafの中を個別に引いていく
				for (value_type const &x : leaf.items) {
					uint32_t v = x.value();
					if (visual_row < v) {
						return std::make_pair(logical, (key_type)visual_row);
					}
					visual_row -= v;
					logical++;
				}
			}
		}
		return std::nullopt;
	}
};

#endif // SOMETHINGMAP_H
