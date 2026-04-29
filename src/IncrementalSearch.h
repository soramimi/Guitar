#ifndef INCREMENTALSEARCH_H
#define INCREMENTALSEARCH_H

#include <string>
#include <optional>
#include <QString>
#include <memory>
#include "MyMecab.h"
#include <regex>

class QRect;
class QPainter;
class QStyleOptionViewItem;

namespace IncrementalSearch {
class AbstractFilter;
}
class IncrementalSearchFilter;


namespace IncrementalSearch {

struct Part {
	bool match = false;
	struct {
		std::string text;
		int pos;
		int end;
	} source;
	struct {
		std::string text;
		int pos;
		int end;
	} kana;
};

struct Result {
	bool match = false;
	size_t pos = 0;
	size_t end = 0;
	std::vector<Part> part;
	operator bool () const
	{
		return match;
	}
};

QString normalizeText(QString s);
void drawText(QPainter *painter, const QStyleOptionViewItem &opt, QRect r, const QString &text);
void drawText_filtered(QPainter *painter, QStyleOptionViewItem const &opt, QRect const &rect, const IncrementalSearchFilter &filter);
void fillFilteredBG(QPainter *painter, const QRect &rect);

class AbstractFilter {
public:
	virtual ~AbstractFilter() = default;
	virtual bool isEmpty() const = 0;
	virtual void makeFilter(std::string const &filtertext) = 0;
	virtual IncrementalSearch::Result match(std::string const &text) const = 0;
};

} // namespace IncrementalSearch


class IncrementalSearchFilter : public IncrementalSearch::AbstractFilter {
private:
	std::shared_ptr<IncrementalSearch::AbstractFilter> filter_p_;
public:
	IncrementalSearchFilter() = default;
	IncrementalSearchFilter(std::shared_ptr<IncrementalSearch::AbstractFilter> const &p)
		: filter_p_(p)
	{
	}
	bool isEmpty() const
	{
		return !filter_p_ || filter_p_->isEmpty();
	}
	void makeFilter(std::string const &filtertext)
	{
		if (!filter_p_) return;
		filter_p_->makeFilter(filtertext);
	}
	IncrementalSearch::Result match(std::string const &text) const
	{
		if (!filter_p_) return {};
		return filter_p_->match(text);
	}
	operator bool () const
	{
		return !isEmpty();
	}
};

class MigemoFilter : public IncrementalSearch::AbstractFilter {
private:
	std::string text_;
	std::shared_ptr<QRegularExpression> re_;
public:
public:
	MigemoFilter() = default;
	MigemoFilter(std::string const &filtertext);
	bool isEmpty() const override;
	void makeFilter(std::string const &filtertext) override;
	IncrementalSearch::Result match(const std::string &text) const override;
};

class MecabFilter : public IncrementalSearch::AbstractFilter {
public:
private:
	std::string original_text_;
	std::string katakana_text_;
	static std::string to_kana(std::string const &text, std::vector<IncrementalSearch::Part> *out);
public:
	MecabFilter() = default;
	MecabFilter(const std::string &filtertext);
	bool isEmpty() const override;
	void makeFilter(std::string const &filtertext) override;
	IncrementalSearch::Result match(const std::string &text) const override;
};

namespace IncrementalSearch {
static inline Result match(std::string const &text, IncrementalSearchFilter const &filter)
{
	return filter.match(text);
}
}

#endif // INCREMENTALSEARCH_H
