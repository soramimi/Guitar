#ifndef USEREVENT_H
#define USEREVENT_H

#include <QEvent>
#include <QVariant>
#include <functional>

enum class UserEventEnum {
	Start = QEvent::User,
	UserFunction,
};
#define UserEvent(e) QEvent::Type(UserEventEnum::e)

class StartEvent : public QEvent {
public:
	StartEvent()
		: QEvent(UserEvent(Start))
	{
	}
};

class UserFunctionEvent : public QEvent {
public:
	std::function<void(QVariant const &, void *ptr)> func;
	QVariant var;
	void *ptr = nullptr;

	explicit UserFunctionEvent(std::function<void(QVariant const &, void *ptr)> func, QVariant const &var, void *ptr = nullptr)
		: QEvent(UserEvent(UserFunction))
		, func(func)
		, var(var)
		, ptr(ptr)
	{
	}
};

#endif // USEREVENT_H
