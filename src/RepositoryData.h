#ifndef REPOSITORYDATA_H
#define REPOSITORYDATA_H

#include <QList>
#include <QMetaType>

enum class ServerType {
	Standard,
	GitHub,
};

struct RepositoryData {
	QString name;
	QString group;
	QString local_dir;
	QString ssh_key;
};
Q_DECLARE_METATYPE(RepositoryData);

class RepositoryBookmark {
public:
	RepositoryBookmark() = default;
	static bool save(QString const &path, QList<RepositoryData> const *items);
	static QList<RepositoryData> load(QString const &path);
};

#endif // REPOSITORYDATA_H
