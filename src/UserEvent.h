#ifndef USEREVENT_H
#define USEREVENT_H

#include <QEvent>
#include <QVariant>
#include <functional>

enum UserEvent {
	Start = QEvent::User,
	EventUserFunction,
};

class StartEvent : public QEvent {
public:
	StartEvent()
		: QEvent((QEvent::Type)UserEvent::Start)
	{
	}
};

class UserFunctionEvent : public QEvent {
public:
	std::function<void(QVariant &)> func;
	QVariant var;

	explicit UserFunctionEvent(std::function<void(QVariant const &)> const &func, QVariant const &var)
		: QEvent((QEvent::Type)EventUserFunction)
		, func(func)
		, var(var)
	{
	}
};


#endif // USEREVENT_H
