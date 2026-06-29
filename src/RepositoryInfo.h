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
	static bool save(QString const &path, const std::vector<RepositoryInfo> &items);
	static std::vector<RepositoryInfo> load(QString const &path);
};

#endif // REPOSITORYINFO_H
