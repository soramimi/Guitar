#ifndef WRAPALGORITHM_H
#define WRAPALGORITHM_H

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

namespace texteditor::wrap_algorithm {

enum class Mode {
	CharWrap,
	WordWrap,
};

using Measure = std::function<int(size_t from, size_t to)>;

/**
 * @brief 論理行を分割するコードポイント位置を求める
 *
 * 戻り値は昇順の終端位置で、空文字列でなければ末尾要素は必ず text.size() になる。
 * measure(from, to) は [from, to) の描画幅を返し、同じ from に対して to が増えると
 * 幅が単調非減少になることを前提とする。
 *
 * WordWrapでも論理行の先頭はCharWrapとして候補を作り、最初の分割可能位置に
 * 到達してからWordWrapへ切り替える。切り替え後に1単語がwidthを超える場合は、
 * その単語の終端候補を採用して幅超過を許容する。
 */
std::vector<size_t> compute_wrap_points(
	std::u32string_view text,
	int width,
	Mode mode,
	Measure const &measure);

}

#endif // WRAPALGORITHM_H
