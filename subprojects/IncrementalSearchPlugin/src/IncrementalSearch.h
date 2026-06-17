#ifndef INCREMENTALSEARCH_H
#define INCREMENTALSEARCH_H

#include "MyMecab.h"
#include <QObject>

class IncrementalSearchFilter {
public:
	std::string original_text;
	std::string katakana_text;
public:
	bool isEmpty() const
	{
		return original_text.empty();
	}
	operator bool () const
	{
		return !isEmpty();
	}
};

class IncrementalSearch : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	std::string to_kana(const std::string &text, std::vector<incrementalsearch::Part> *out) const;
public:
	IncrementalSearch();
	~IncrementalSearch();
	virtual bool open();
	virtual std::string convertRomajiToKatakana(std::string const &a);
	virtual IncrementalSearchFilter makeFilter(const std::string &filtertext);
	virtual incrementalsearch::Result match(const std::string &text, const IncrementalSearchFilter &filter) const;
};

#endif // INCREMENTALSEARCH_H
