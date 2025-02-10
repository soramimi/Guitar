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

class CloneRepositoryEventData {
public:
	QString remote_url;
	CloneRepositoryEventData(QString const &remote_url)
		: remote_url(remote_url)
	{
	}
};

class UserEventHandler {
public:
	typedef std::variant<
		StartEventData,
		CloneRepositoryEventData
	> variant_t;

	MainWindow *mainwindow;

	UserEventHandler(MainWindow *mw)
		: mainwindow(mw)
	{
	}

	void operator () (StartEventData const &e);
	void operator () (CloneRepositoryEventData const &e);

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
