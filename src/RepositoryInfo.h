#ifndef REPOSITORYINFO_H
#define REPOSITORYINFO_H

#include <QList>
#include <QMetaType>

struct RepositoryInfo {
	QString name;
	QString group;
	QString local_dir;
	QString ssh_key;
};
Q_DECLARE_METATYPE(RepositoryInfo)

class RepositoryBookmark {
public:
	RepositoryBookmark() = default;
	static bool save(QString const &path, QList<RepositoryInfo> const *items);
	static QList<RepositoryInfo> load(QString const &path);
};

#endif // REPOSITORYINFO_H
