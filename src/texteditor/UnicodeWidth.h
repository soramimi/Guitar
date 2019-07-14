#ifndef UNICODEWIDTH_H
#define UNICODEWIDTH_H

#include <cstdint>



class UnicodeWidth {
public:
	enum class Type {
		Unknown,
		Neutral,
		Narrow,
		Half,
		Full,
		Wide,
		Ambigous,
	};
private:
	struct Range {
		uint32_t lo;
		uint32_t hi;
		Type type;
	};
public:
	UnicodeWidth() = delete;
	static Type type(uint32_t c);
	static int width(Type t)
	{
		switch (t) {
		case Type::Neutral:
		case Type::Narrow:
		case Type::Half:
			return 1;
		case Type::Full:
		case Type::Wide:
		case Type::Ambigous:
			return 2;
		}
		return 0;
	}
};

#endif // UNICODEWIDTH_H
