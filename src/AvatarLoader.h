#ifndef AVATARLOADER_H
#define AVATARLOADER_H

#include <QIcon>
#include <QObject>
#include <deque>
#include <set>
#include <string>

class MainWindow;
class WebContext;

class AvatarLoader : public QObject {
	Q_OBJECT
public:
	struct Item {
		std::string email;
		QImage image;
	};
private:
	enum State {
		Idle,
		Busy,
		Done,
		Fail,
	};
	struct RequestItem {
		State state = Idle;
		QString email;
		QImage image;
	};
	struct Private;
	Private *m;

	bool isInterruptionRequested() const;
protected:
	void run();
public:
	AvatarLoader(QObject *parent = nullptr);
	~AvatarLoader() override;
	void requestInterruption();
	QImage fetch(const QString &email, bool request) const;
	void stop();
	void start(MainWindow *mainwindow);
	template <typename Func2> void connectAvatarReady(const typename QtPrivate::FunctionPointer<Func2>::Object *receiver, Func2 slot)
	{
		connect(this, &AvatarLoader::ready, receiver, slot);
	}
	template <typename Func2> void disconnectAvatarReady(const typename QtPrivate::FunctionPointer<Func2>::Object *receiver, Func2 slot)
	{
		disconnect(this, &AvatarLoader::ready, receiver, slot);
	}
signals:
	void ready();
};

#endif // AVATARLOADER_H
