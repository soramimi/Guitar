#ifndef USEREVENT_H
#define USEREVENT_H

#include <QEvent>
#include <QVariant>
#include <functional>

enum class UserEvent {
	Start = QEvent::User,
	UserFunction,
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
	std::function<void(QVariant const &, void *ptr)> func;
	QVariant var;
	void *ptr = nullptr;

	explicit UserFunctionEvent(std::function<void(QVariant const &, void *ptr)> const &func, QVariant const &var, void *ptr = nullptr)
		: QEvent((QEvent::Type)UserEvent::UserFunction)
		, func(func)
		, var(var)
		, ptr(ptr)
	{
	}
};

#endif // USEREVENT_H
