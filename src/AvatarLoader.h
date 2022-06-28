#ifndef AVATARLOADER_H
#define AVATARLOADER_H

#include <QIcon>
#include <deque>
#include <set>
#include <string>

class MainWindow;
class WebContext;

class AvatarLoader {
private:
	enum State {
		Idle,
		Busy,
		Done,
	};
	struct RequestItem {
		State state = Idle;
		std::string email;
		QImage image;
	};
	struct Private;
	Private *m;

	bool isInterruptionRequested() const;
protected:
	void run();
public:
	AvatarLoader();
	~AvatarLoader();
	void requestInterruption();
	QIcon fetch(std::string const &email, bool request) const;
	void stop();
	void start(MainWindow *mainwindow);
	void addListener(QObject *listener);
	void removeListener(QObject *listener);
};

#endif // AVATARLOADER_H
