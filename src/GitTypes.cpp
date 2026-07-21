
#include "GitTypes.h"
#include "GitCommitItem.h"
#include "GitHash.h"
#include "GitUser.h"
#include <common/crc32.h>
#include <common/misc.h>
#include <common/q/helper.h>
#include <deque>
#include <set>

class Latin1View {
private:
	QString const &text_;
public:
	Latin1View(QString const &s)
		: text_(s)
	{
	}
	bool empty() const
	{
		return text_.isEmpty();
	}
	size_t size() const
	{
		return text_.size();
	}
	char operator [](int i) const
	{
		return (i >= 0 && i < text_.size()) ? text_.utf16()[i] : 0;
	}
};

// GitUser

GitUser::operator bool() const
{
	return misc::isValidMailAddress(email);
}

// GitHash

GitHash::GitHash()
{
}

GitHash::GitHash(const std::string_view &id)
{
	assign(id);
}

GitHash::GitHash(const char *id)
{
	assign(std::string_view(id, strlen(id)));
}

template <typename VIEW> void GitHash::_assign(VIEW const &id)
{
	if (id.empty()) {
		valid_ = false;
	} else if (id == "HEAD") {
		valid_ = false;
	} else {
		valid_ = true;
		if (id.size() == GIT_ID_LENGTH) {
			char tmp[3];
			tmp[2] = 0;
			for (int i = 0; i < GIT_ID_LENGTH / 2; i++) {
				unsigned char c = id[i * 2 + 0];
				unsigned char d = id[i * 2 + 1];
				if (std::isxdigit(c) && std::isxdigit(d)) {
					tmp[0] = c;
					tmp[1] = d;
					id_[i] = strtol(tmp, nullptr, 16);
				} else {
					valid_ = false;
					break;
				}
			}
		}
	}
}

void GitHash::assign(const std::string_view &id)
{
	_assign(id);
}

std::string GitHash::toString(int maxlen) const
{
	if (valid_) {
		char tmp[GIT_ID_LENGTH + 1];
		for (int i = 0; i < GIT_ID_LENGTH / 2; i++) {
			sprintf(tmp + i * 2, "%02x", id_[i]);
		}
		if (maxlen < 0 || maxlen > GIT_ID_LENGTH) {
			maxlen = GIT_ID_LENGTH;
		}
		tmp[maxlen] = 0;
		return std::string(tmp, maxlen);
	}
	return {};
}

bool GitHash::isValid() const
{
	if (!valid_) return false;
	uint8_t c = 0;
	for (std::size_t i = 0; i < sizeof(id_); i++) {
		c |= id_[i];
	}
	return c != 0; // すべて0ならfalse
}

int GitHash::compare(const GitHash &other) const
{
	if (!valid_ && other.valid_) return -1;
	if (valid_ && !other.valid_) return 1;
	if (!valid_ && !other.valid_) return 0;
	return memcmp(id_, other.id_, sizeof(id_));
}

GitHash::operator bool() const
{
	return isValid();
}

size_t GitHash::_std_hash() const
{
	return crc32(0, id_, sizeof(id_));
}

bool GitHash::isValidID(std::string const &id)
{
	int zero = 0;
	size_t n = id.size();
	if (n >= 4 && n <= GIT_ID_LENGTH) {
		char const *p = id.c_str();
		for (size_t i = 0; i < n; i++) {
			char c = p[i];
			if (c == '0') {
				zero++;
			} else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
				// ok
			} else {
				return false;
			}
		}
		if (zero == GIT_ID_LENGTH) { // 全部 0 の時は false を返す
			return false;
		}
		return true; // ok
	}
	return false;
}

//

std::string gitTrimPath(std::string const &s)
{
	char const *begin = s.data();
	char const *end = begin + s.size();
	char const *left = begin;
	char const *right = end;
	while (left < right && isspace((unsigned char)*left)) left++;
	while (left < right && isspace((unsigned char)right[-1])) right--;
	if (left + 1 < right && *left == '\"' && right[-1] == '\"') { // if quoted ?
		left++;
		right--;
		std::string ba;
		char const *ptr = left;
		while (ptr < right) {
			char c = *ptr;
			ptr++;
			if (c == '\\') {
				if (ptr + 2 < right && ptr[0] == '\\' && ptr[1] == '\\') { // triple backslash
					c = ptr[2];
					ptr += 3;
				} else if (ptr < end && isdigit((unsigned char)*ptr)) { // octal number
					c = 0;
					for (int i = 0; i < 3 && ptr < right && isdigit((unsigned char)*ptr); i++) { // decode \oct
						c = c * 8 + (*ptr - '0');
						ptr++;
					}
				}
			}
			ba += c;
		}
		return ba;
	}
	if (left == begin && right == end) return s;
	return {left, size_t(right - left)};
}

// GitCommitItemList

struct GitCommitItemList::Private {
	struct D {
		std::vector<GitCommitItem> list;
		mutable std::vector<GitCommitItem *> ptrs;
		std::map<GitHash, size_t> map;
	} d;
};

GitCommitItemList::~GitCommitItemList()
{
	delete m;
}

GitCommitItemList::GitCommitItemList()
	: m(new Private)
{
}

GitCommitItemList::GitCommitItemList(const GitCommitItemList &r)
	: m(new Private)
{
	assign(r);
}

GitCommitItemList::GitCommitItemList(GitCommitItemList &&r)
	: m(new Private)
{
	m->d = std::move(r.m->d);
}

GitCommitItemList::GitCommitItemList(std::vector<GitCommitItem> &&list)
	: m(new Private)
{
	setList(std::move(list));
}

void GitCommitItemList::_update_ptrs()
{
	if (m->d.ptrs.empty()) {
		const size_t N = m->d.list.size();
		m->d.ptrs.resize(N);
		for (size_t i = 0; i < N; i++) {
			m->d.ptrs[i] = &m->d.list[i];
		}
	}
}

void GitCommitItemList::assign(const GitCommitItemList &r)
{
	m->d.list = r.m->d.list;
	m->d.ptrs.clear();
	m->d.map = r.m->d.map;
}

void GitCommitItemList::setList(std::vector<GitCommitItem> &&list)
{
	m->d.list = std::move(list);
	m->d.ptrs.clear();
}

size_t GitCommitItemList::size() const
{
	return m->d.list.size();
}

GitCommitItem &GitCommitItemList::_at(size_t i)
{
	return m->d.list.at(i);
}

const GitCommitItem &GitCommitItemList::at(size_t i) const
{
	return m->d.list.at(i);
}

const GitCommitItem &GitCommitItemList::operator [](size_t i) const
{
	return at(i);
}

bool GitCommitItemList::empty() const
{
	return m->d.list.empty();
}

void GitCommitItemList::push_front(const GitCommitItem &item)
{
	m->d.list.insert(m->d.list.begin(), item);
	m->d.ptrs.clear();
}

std::string GitCommitItemList::previousMessage() const
{
	for (GitCommitItem const &item : m->d.list) {
		if (item.commit_id.isValid()) {
			return item.message;
		}
	}
	return {};
}

void GitCommitItemList::updateIndex()
{
	m->d.map.clear();
	for (size_t i = 0; i < m->d.list.size(); i++) {
		m->d.map[m->d.list[i].commit_id] = i;
	}
}

int GitCommitItemList::find_index(const GitHash &id) const
{
	if (id.isValid()) {
		auto it = m->d.map.find(id);
		if (it != m->d.map.end()) {
			return (int)it->second;
		}
	}
	return -1;
}

// GitCommitItem *GitCommitItemList::_find(const GitHash &id)
// {
// 	// int index = find_index(id);
// 	// if (index >= 0) {
// 	// 	return &m->d.list[index];
// 	// }
// 	return nullptr;
// }

const GitCommitItem *GitCommitItemList::find(const GitHash &id) const
{
	int index = find_index(id);
	if (index >= 0) {
		return &m->d.list[index];
	}
	return nullptr;
}

std::span<const GitCommitItem *const> GitCommitItemList::items() const
{
	const_cast<GitCommitItemList *>(this)->_update_ptrs();
	return std::span<GitCommitItem const *const>(m->d.ptrs.data(), m->d.ptrs.size());
}

std::span<const GitCommitItem *const> GitCommitItemList::c_items() const
{
	const_cast<GitCommitItemList *>(this)->_update_ptrs();
	return std::span<GitCommitItem const *const>(m->d.ptrs.data(), m->d.ptrs.size());
}

void GitCommitItemList::fixCommitLogOrder()
{
	std::vector<GitCommitItem> list1;
	
	std::swap(list1, m->d.list);

	const size_t count = list1.size();

	std::vector<size_t> index(count);
	std::iota(index.begin(), index.end(), 0);

	auto ListItem = [&](size_t i)->GitCommitItem const &{
		return list1.at(index[i]);
	};

	auto MoveItem = [&](size_t from, size_t to){
		Q_ASSERT(from > to);
		// 親子関係が整合しないコミットをリストの上の方へ移動する
		// ListItem(from).order_fixed = true;
		auto a = index[from];
		index.erase(index.begin() + from);
		index.insert(index.begin() + to, a);
	};

	// 親子関係を調べて、順番が狂っていたら、修正する。

	std::set<GitHash> set;
	size_t limit = count;
	size_t i = 0;
	while (i < count) {
		size_t newpos = (size_t)-1;
		for (GitHash const &parent : ListItem(i).parent_ids) {
			if (set.find(parent) != set.end()) {
				for (size_t j = 0; j < i; j++) {
					if (parent == ListItem(j).commit_id) {
						if (newpos == (size_t)-1 || j < newpos) {
							newpos = j;
						}
						// qDebug() << "fix commit order" << list[i].commit_id.toQString();
						break;
					}
				}
			}
		}
		set.insert(set.end(), ListItem(i).commit_id);
		if (newpos != (size_t)-1) {
			if (limit == 0) break; // まず無いと思うが、もし、無限ループに陥ったら
			MoveItem(i, newpos);
			i = newpos;
			limit--;
		}
		i++;
	}

	//
	
	std::vector<GitCommitItem> list2;
	list2.reserve(count);
	for (size_t i : index) {
		list2.push_back(ListItem(i));
	}
	
	setList(std::move(list2));
}

/**
 * @brief MainWindow::updateCommitGraph
 *
 * 樹形図情報を構築する
 */
void GitCommitItemList::updateCommitGraph()
{
	const int LogCount = (int)this->size();
	if (LogCount > 0) {
		auto LogItem = [&](int i)->GitCommitItem &{
			return this->_at((size_t)i);
		};
		enum { // 有向グラフを構築するあいだ CommitItem::marker_depth をフラグとして使用する
			UNKNOWN = 0,
			KNOWN = 1,
		};
		for (GitCommitItem &item : this->m->d.list) {
			item.marker_depth = UNKNOWN;
		}
		// コミットハッシュを検索して、親コミットのインデックスを求める
		for (int i = 0; i < LogCount; i++) {
			GitCommitItem *item = &LogItem(i);
			item->parent_lines.clear();
			if (item->parent_ids.empty()) {
				item->resolved = true;
			} else {
				for (size_t j = 0; j < item->parent_ids.size(); j++) { // 親の数だけループ
					GitHash const &parent_id = item->parent_ids[j]; // 親のハッシュ値
					for (int k = i + 1; k < (int)LogCount; k++) { // 親を探す
						if (LogItem(k).commit_id == parent_id) { // ハッシュ値が一致したらそれが親
							item->parent_lines.emplace_back(k); // インデックス値を記憶
							LogItem(k).has_child = true;
							LogItem(k).marker_depth = KNOWN;
							item->resolved = true;
							break;
						}
					}
				}
			}
		}

		struct Element {
			int depth = 0;
			std::vector<int> indexes;
		};

		struct Task {
			int index = 0;
			int parent = 0;
			Task() = default;
			Task(int index, int parent)
				: index(index)
				, parent(parent)
			{
			}
		};

		std::vector<Element> elements; // 線分リスト
		{ // 線分リストを作成する
			std::deque<Task> tasks; // 未処理タスクリスト
			{
				for (int i = 0; i < LogCount; i++) {
					GitCommitItem *item = &LogItem((int)i);
					if (item->marker_depth == UNKNOWN) {
						int n = (int)item->parent_lines.size(); // 最初のコミットアイテム
						for (int j = 0; j < n; j++) {
							tasks.emplace_back(i, j); // タスクを追加
						}
					}
					item->marker_depth = UNKNOWN;
				}
			}
			while (!tasks.empty()) { // タスクが残っているならループ
				Element e;
				Task task;
				{ // 最初のタスクを取り出す
					task = tasks.front();
					tasks.pop_front();
				}
				e.indexes.push_back(task.index); // 先頭のインデックスを追加
				int index = LogItem(task.index).parent_lines[task.parent].index; // 開始インデックス
				while (index > 0 && index < LogCount) { // 最後に到達するまでループ
					e.indexes.push_back(index); // インデックスを追加
					size_t n = LogItem(index).parent_lines.size(); // 親の数
					if (n == 0) break; // 親がないなら終了
					GitCommitItem *item = &LogItem(index);
					if (item->marker_depth == KNOWN) break; // 既知のアイテムに到達したら終了
					item->marker_depth = KNOWN; // 既知のアイテムにする
					for (int i = 1; i < (int)n; i++) {
						tasks.emplace_back(index, i); // タスク追加
					}
					index = LogItem(index).parent_lines[0].index; // 次の親（親リストの先頭の要素）
				}
				if (e.indexes.size() >= 2) {
					elements.push_back(e);
				}
			}
		}
		// 線情報をクリア
		for (GitCommitItem &item : this->m->d.list) {
			item.marker_depth = -1;
			item.parent_lines.clear();
		}
		// マークと線の深さを決める
		if (!elements.empty()) {
			{ // 優先順位を調整する
				std::sort(elements.begin(), elements.end(), [](Element const &left, Element const &right){
					int i = 0;
					{ // 長いものを優先して左へ
						int l = left.indexes.back() - left.indexes.front();
						int r = right.indexes.back() - right.indexes.front();
						i = r - l; // 降順
					}
					if (i == 0) {
						// コミットが新しいものを優先して左へ
						int l = left.indexes.front();
						int r = right.indexes.front();
						i = l - r; // 昇順
					}
					return i < 0;
				});
				// 子の無いブランチ（タグ等）が複数連続しているとき、古いコミットを右に寄せる
				{
					for (int i = 0; i + 1 < (int)elements.size(); i++) {
						Element *e = &elements[i];
						int index1 = e->indexes.front();
						if (index1 > 0 && !LogItem(index1).has_child) { // 子がない
							// 新しいコミットを探す
							for (int j = i + 1; j < (int)elements.size(); j++) { // 現在位置より後ろを探す
								Element *f = &elements[j];
								int index2 = f->indexes.front();
								if (index1 == index2 + 1) { // 一つだけ新しいコミット
									Element t = std::move(*f);
									elements.erase(elements.begin() + j); // 移動元を削除
									elements.insert(elements.begin() + i, std::move(t)); // 現在位置に挿入
								}
							}
							// 古いコミットを探す
							int j = 0;
							while (j < i) { // 現在位置より前を探す
								Element *f = &elements[j];
								int index2 = f->indexes.front();
								if (index1 + 1 == index2) { // 一つだけ古いコミット
									Element t = std::move(*f);
									elements.erase(elements.begin() + j); // 移動元を削除
									elements.insert(elements.begin() + i, std::move(t)); // 現在位置の次に挿入
									index1 = index2;
								} else {
									j++;
								}
							}
						}
					}
				}
			}
			{ // 最初の線は深さを0にする
				Element *e = &elements.front();
				for (size_t i = 0; i < e->indexes.size(); i++) {
					int index = e->indexes[i];
					LogItem(index).marker_depth = 0; // マークの深さを設定
					e->depth = 0; // 線の深さを設定
				}
			}
			// 最初以外の線分の深さを決める
			for (size_t i = 1; i < elements.size(); i++) { // 最初以外をループ
				Element *e = &elements[i];
				int depth = 1;
				while (1) { // 失敗したら繰り返し
					for (size_t j = 0; j < i; j++) { // 既に処理済みの線を調べる
						Element const *f = &elements[j]; // 検査対象
						if (e->indexes.size() == 2) { // 二つしかない場合
							int from = e->indexes[0]; // 始点
							int to = e->indexes[1];   // 終点
							if (LogItem(from).has_child) {
								for (size_t k = 0; k + 1 < f->indexes.size(); k++) { // 検査対象の全ての線分を調べる
									int curr = f->indexes[k];
									int next = f->indexes[k + 1];
									if (from > curr && to == next) { // 決定済みの線に直結できるか判定
										e->indexes.back() = from + 1; // 現在の一行下に直結する
										e->depth = elements[j].depth; // 接続先の深さ
										goto DONE; // 決定
									}
								}
							}
						}
						if (depth == f->depth) { // 同じ深さ
							if (e->indexes.back() > f->indexes.front() && e->indexes.front() < f->indexes.back()) { // 重なっている
								goto FAIL; // この深さには線を置けないのでやりなおし
							}
						}
					}
					for (size_t j = 0; j < e->indexes.size(); j++) {
						int index = e->indexes[j];
						GitCommitItem *item = &LogItem(index);
						if (j == 0 && item->has_child) { // 最初のポイントで子がある場合
							// nop
						} else if ((j > 0 && j + 1 < e->indexes.size()) || item->marker_depth < 0) { // 最初と最後以外、または、未確定の場合
							item->marker_depth = depth; // マークの深さを設定
						}
					}
					e->depth = depth; // 深さを決定
					goto DONE; // 決定
FAIL:;
					depth++; // 一段深くして再挑戦
				}
DONE:;
			}
			// 線情報を生成する
			for (auto &e : elements) {
				auto ColorNumber = [&](){ return e.depth; };
				size_t count = e.indexes.size();
				for (size_t i = 0; i + 1 < count; i++) {
					int curr = e.indexes[i];
					int next = e.indexes[i + 1];
					GitTreeLine line(next, e.depth);
					line.color_number = ColorNumber();
					line.bend_early = (i + 2 < count || !LogItem(next).resolved);
					if (i + 2 == count) {
						int join = false;
						if (count > 2) { // 直結ではない
							join = true;
						} else if (!LogItem(curr).has_child) { // 子がない
							join = true;
							int d = LogItem(curr).marker_depth; // 開始点の深さ
							for (int j = curr + 1; j < next; j++) {
								GitCommitItem *item = &LogItem(j);
								if (item->marker_depth == d) { // 衝突する
									join = false; // 迂回する
									break;
								}
							}
						}
						if (join) {
							line.depth = LogItem(next).marker_depth; // 合流する先のアイテムの深さと同じにする
						}
					}
					LogItem(curr).parent_lines.push_back(line); // 線を追加
				}
			}
		} else {
			if (LogCount == 1) { // コミットが一つだけ
				LogItem(0).marker_depth = 0;
			}
		}
	}
}

