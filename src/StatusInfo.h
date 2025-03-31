#ifndef STATUSINFO_H
#define STATUSINFO_H

#include <QMetaType>
#include <QString>
#include <optional>

class StatusInfo {
public:
	struct Message {
		QString text;
		Qt::TextFormat format = Qt::PlainText;
		Message() = default;
		Message(QString const &text, Qt::TextFormat format = Qt::PlainText)
			: text(text)
			, format(format)
		{}
	};
	std::optional<Message> message_;
	std::optional<float> progress_;
private:
	StatusInfo(std::optional<Message> message, std::optional<float> progress)
		: message_(message), progress_(progress)
	{
	}
public:
	StatusInfo() = default;
	static StatusInfo Clear()
	{
		return StatusInfo(std::nullopt, std::nullopt);
	}
	static StatusInfo message(Message const &msg)
	{
		return StatusInfo(msg, std::nullopt);
	}
	static StatusInfo progress(Message const &msg, float progress = -1.0f)
	{
		return StatusInfo(msg, progress);
	}
	static StatusInfo progress(float progress)
	{
		return StatusInfo(std::nullopt, progress);
	}
};
Q_DECLARE_METATYPE(StatusInfo)

#endif // STATUSINFO_H
