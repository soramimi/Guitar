#ifndef GITOBJECTMANAGER_H
#define GITOBJECTMANAGER_H

#include <QString>

class GitObjectManager {
private:
	QString working_dir;
	bool loadObject_(const QString &id, QByteArray *out);
	bool extractObjectFromPackFile_(const QString &id, QByteArray *out);
public:
	GitObjectManager(QString const &workingdir)
		: working_dir(workingdir)
	{
	}
	bool loadObjectFile(const QString &id, QByteArray *out)
	{
		if (loadObject_(id, out)) return true;
		if (extractObjectFromPackFile_(id, out)) return true;
		return false;
	}
};

#endif // GITOBJECTMANAGER_H
