#ifndef INCREMENTALSEARCH_H
#define INCREMENTALSEARCH_H

#include <string>
#include <optional>
#include <QString>
#include <memory>
#include "LibMecab.h"

class QRect;
class QPainter;
class QStyleOptionViewItem;

class AbstractIncrementalSearchFilter;
class IncrementalSearchFilter;


namespace IncrementalSearch {

QString normalizeText(QString s);
void drawText(QPainter *painter, const QStyleOptionViewItem &opt, QRect r, const QString &text);
void drawText_filtered(QPainter *painter, QStyleOptionViewItem const &opt, QRect const &rect, const IncrementalSearchFilter &filter);
void fillFilteredBG(QPainter *painter, const QRect &rect);

}

class AbstractIncrementalSearchFilter {
public:
	struct Part2 {
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
		std::vector<Part2> part2;
		operator bool () const
		{
			return match;
		}
	};

	virtual ~AbstractIncrementalSearchFilter() = default;
	virtual bool isEmpty() const = 0;
	virtual void makeFilter(const QString &filtertext) = 0;
	virtual Result match(QString const &text) const = 0;

};

class IncrementalSearchFilter : public AbstractIncrementalSearchFilter {
private:
	std::shared_ptr<AbstractIncrementalSearchFilter> filter_ptr_;
public:
	IncrementalSearchFilter() = default;
	IncrementalSearchFilter(std::shared_ptr<AbstractIncrementalSearchFilter> const &p)
		: filter_ptr_(p)
	{
	}
	bool isEmpty() const
	{
		return !filter_ptr_ || filter_ptr_->isEmpty();
	}
	void makeFilter(const QString &filtertext)
	{
		if (!filter_ptr_) return;
		filter_ptr_->makeFilter(filtertext);
	}
	Result match(const QString &text) const
	{
		if (!filter_ptr_) return {};
		return filter_ptr_->match(text);
	}
	operator bool () const
	{
		return !isEmpty();
	}
};

class MigemoFilter : public AbstractIncrementalSearchFilter {
private:
	QString text_;
	std::shared_ptr<QRegularExpression> re_;
public:
public:
	MigemoFilter() = default;
	MigemoFilter(const QString &filtertext);
	bool isEmpty() const override;
	void makeFilter(const QString &filtertext) override;
	Result match(QString const &text) const override;
};

class MecabFilter : public AbstractIncrementalSearchFilter {
public:
private:
	std::string original_text_;
	std::string katakana_text_;
	static std::string to_kana(std::string const &text, std::vector<Part2> *out);
public:
	MecabFilter() = default;
	MecabFilter(QString const &filtertext);
	bool isEmpty() const override;
	void makeFilter(QString const &filtertext) override;
	Result match(QString const &text) const override;
};


#endif // INCREMENTALSEARCH_H
