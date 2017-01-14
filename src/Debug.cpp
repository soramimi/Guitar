
#include "Debug.h"
#include <QDebug>
#include <QFile>
#include "GitPack.h"




void Debug::doit()
{
	char const *idx_path = "C:/develop/GetIt/.git/objects/pack/pack-da889d867e8acb4d18c95ed6d519c5609e0e78d5.idx";
	GitPackIdxV2 idx;
	idx.parse(idx_path);
	int n = idx.count();
	for (int i = 0; i < n; i++) {
		GitPackIdxV2::Item const *item = idx.item(i);
		//	qDebug() << idx.offset(0);
		//	qDebug() << idx.offset(1);
		char const *pack_path = "C:/develop/GetIt/.git/objects/pack/pack-da889d867e8acb4d18c95ed6d519c5609e0e78d5.pack";
		GitPack::Object t;
		GitPack::load(pack_path, item, &t);
		qDebug() << t.offset;
	}
}
