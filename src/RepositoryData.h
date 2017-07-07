#ifndef REPOSITORYDATA_H
#define REPOSITORYDATA_H

#include <QList>

enum class ServerType {
	Standard,
	GitHub,
};

struct RepositoryItem {
	QString name;
	QString group;
	QString local_dir;
};

class RepositoryBookmark {
public:
	RepositoryBookmark();
	static bool save(QString const &path, QList<RepositoryItem> const *items);
	static QList<RepositoryItem> load(QString const &path);
};

#endif // REPOSITORYDATA_H
