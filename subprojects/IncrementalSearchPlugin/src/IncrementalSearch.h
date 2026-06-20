#ifndef INCREMENTALSEARCH_H
#define INCREMENTALSEARCH_H

// #include "MyMecab.h"
#include <QObject>

//

namespace incrementalsearch {

struct Element {
	bool match = false;
	struct Fragment {
		std::string text;
		size_t pos = 0;
		size_t end = 0;
	};
	Fragment source;
	Fragment kana;
};


struct Result {
	struct Part : public Element::Fragment {
		bool match = false;
		Part() = default;
		Part(Element const &t)
			: Element::Fragment(t.source)
			, match(t.match)
		{
		}
	};
	bool match = false;
	std::vector<Part> parts;
	explicit operator bool () const
	{
		return match;
	}
};

struct Part {
	size_t offset;
	size_t length;
	std::string text;
};

class Engine {
private:
	struct Private;
	Private *m;
public:
public:
	virtual ~Engine() {}
	virtual bool open(char const *dicpath) = 0;
	virtual void close() = 0;
	virtual std::vector<incrementalsearch::Part> parse(std::string_view const &line) const = 0;
	virtual operator bool () const = 0;
};

} // namespace incrementalsearch

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
	incrementalsearch::Engine *mecab();
	incrementalsearch::Engine const *mecab() const;
	std::string to_kana(const std::string &text, std::vector<incrementalsearch::Element> *out) const;
public:
	IncrementalSearch();
	~IncrementalSearch();
	virtual bool open();
	virtual std::string convertRomajiToKatakana(std::string const &a);
	virtual IncrementalSearchFilter makeFilter(const std::string &filtertext);
	virtual incrementalsearch::Result match(const std::string &text, const IncrementalSearchFilter &filter) const;
};

#endif // INCREMENTALSEARCH_H
