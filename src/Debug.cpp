
#include "Debug.h"
#include "GitPackIdxV2.h"
#include <QDebug>
#include <QFile>

void Debug::doit()
{
	char const *path = "C:/develop/GetIt/.git/objects/pack/pack-da889d867e8acb4d18c95ed6d519c5609e0e78d5.idx";
		QFile file(path);
		if (file.open(QFile::ReadOnly)) {
			GitPackIdxV2 idx;
			qDebug() << idx.parse(&file);
		}
}

