
#ifndef INCREMENTALSEARCHFILTER_H
#define INCREMENTALSEARCHFILTER_H

#include <QString> // include first

#define XSTRING_WITH_STD_STRING
#include "../../xstring.h"

class IncrementalSearchFilter {
public:
	xstring original_text;
	xstring katakana_text;
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
