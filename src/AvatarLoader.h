#ifndef AVATARLOADER_H
#define AVATARLOADER_H

#include "GitHubAPI.h"
#include <QIcon>
#include <QThread>
#include <QMutex>
#include <deque>
#include <QWaitCondition>
#include <set>
#include <string>

class WebContext;

class AvatarLoader : public QThread {
	Q_OBJECT
private:
	struct RequestItem {
		std::string email;
		QIcon icon;
	};
	struct Private;
	Private *m;

protected:
	void run() override;
public:
	AvatarLoader();
	~AvatarLoader() override;
	QIcon fetch(std::string const &email, bool request) const;
	void stop();
	void start(WebContext *webcx);
signals:
	void updated();
};

#endif // AVATARLOADER_H
