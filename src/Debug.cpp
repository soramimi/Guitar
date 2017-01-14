
#include "Debug.h"
#include <QBuffer>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include "Git.h"
#include "GitPack.h"
#include "joinpath.h"
#include "GitObjectManager.h"


void Debug::doit2()
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


void Debug::doit(QString const &workingdir)
{
	QByteArray out;
	QString id = "aa2d";
	GitObjectManager gfm(workingdir);
	gfm.loadObjectFile(id, &out);
	qDebug() << out.size();
}
