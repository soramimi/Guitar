
#ifndef INCREMENTALSEARCHFILTER_H
#define INCREMENTALSEARCHFILTER_H

#include <string>

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


#endif // INCREMENTALSEARCHFILTER_H
