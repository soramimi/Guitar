#ifndef USEREVENT_H
#define USEREVENT_H

#include <QEvent>
#include <QVariant>
#include <functional>

class MainWindow;
class UserEvent;

enum {
	UserEventType = QEvent::User,
};

class StartEventData {
public:
	StartEventData()
	{
	}
};

class AddRepositoryEventData {
public:
	QString dir;
	AddRepositoryEventData(QString dir)
		: dir(dir)
	{
	}
};

class UpdateFileListEventData {
public:
	UpdateFileListEventData()
	{
	}
};

class UserEventHandler {
public:
	typedef std::variant<
		StartEventData,
		AddRepositoryEventData,
		UpdateFileListEventData
	> variant_t;

	MainWindow *mainwindow;

	UserEventHandler(MainWindow *mw)
		: mainwindow(mw)
	{
	}

	void operator () (StartEventData const &e);
	void operator () (AddRepositoryEventData const &e);
	void operator () (UpdateFileListEventData const &e);

	void go(UserEvent *e);
};

class UserEvent : public QEvent {
public:
	UserEventHandler::variant_t data_;

	UserEvent(UserEventHandler::variant_t &&v)
		: QEvent((QEvent::Type)UserEventType)
		, data_(v)
	{
	}
};

inline void UserEventHandler::go(UserEvent *e)
{
	std::visit(*this, e->data_);
}

#endif // USEREVENT_H
