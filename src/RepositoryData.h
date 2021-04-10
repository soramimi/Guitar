#ifndef REPOSITORYDATA_H
#define REPOSITORYDATA_H

#include <QList>
#include <QMetaType>

enum class ServerType {
	Standard,
	GitHub,
};

struct RepositoryItem {
	QString name;
	QString group;
	QString local_dir;
	QString ssh_key;
};
Q_DECLARE_METATYPE(RepositoryItem);

class RepositoryBookmark {
public:
	RepositoryBookmark() = default;
	static bool save(QString const &path, QList<RepositoryItem> const *items);
	static QList<RepositoryItem> load(QString const &path);
};

#endif // REPOSITORYDATA_H
