#ifndef AVATARLOADER_H
#define AVATARLOADER_H

#include "GitHubAPI.h"
#include "RepositoryWrapperFrame.h"
#include <QIcon>
#include <QThread>
#include <QMutex>
#include <deque>
#include <set>
#include <string>

class MainWindow;
class WebContext;

class AvatarLoader : public QThread {
	Q_OBJECT
private:
	struct RequestItem {
		RepositoryWrapperFrame *frame = nullptr;
		std::string email;
		QImage image;
	};
	struct Private;
	Private *m;

protected:
	void run() override;
public:
	AvatarLoader();
	~AvatarLoader() override;
	QIcon fetch(RepositoryWrapperFrame *frame, std::string const &email, bool request) const;
	void stop();
	void start(MainWindow *mainwindow);
signals:
	void updated(RepositoryWrapperFrameP frame);
};

#endif // AVATARLOADER_H
