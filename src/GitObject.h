
#ifndef GITOBJECT_H
#define GITOBJECT_H

#include <QByteArray>

struct GitObject {
	enum class Type { // 値は固定。packフォーマットで決まってる
		NONE = -1,
		UNKNOWN = 0,
		COMMIT = 1,
		TREE = 2,
		BLOB = 3,
		TAG = 4,
		UNDEFINED = 5,
		OFS_DELTA = 6,
		REF_DELTA = 7,
	};
	Type type = Type::NONE;
	QByteArray content;
	explicit operator bool () const
	{
		return type != Type::NONE;
	}
};


#endif // GITOBJECT_H
