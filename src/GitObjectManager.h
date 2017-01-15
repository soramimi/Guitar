#ifndef GITOBJECTMANAGER_H
#define GITOBJECTMANAGER_H

#include <QString>

class GitObjectManager {
private:
	QString working_dir;
	bool loadObject_(const QString &id, QByteArray *out);
	bool extractObjectFromPackFile_(const QString &id, QByteArray *out);
public:
	GitObjectManager(QString const &workingdir);
	bool loadObjectFile(const QString &id, QByteArray *out);
};

#endif // GITOBJECTMANAGER_H
