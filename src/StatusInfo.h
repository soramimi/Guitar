#ifndef STATUSINFO_H
#define STATUSINFO_H

#include <QMetaType>
#include <QString>
#include <optional>

class StatusInfo {
public:
	std::optional<QString> message;
	std::optional<float> progress;
private:
	StatusInfo(std::optional<QString> message, std::optional<float> progress)
		: message(message), progress(progress)
	{
	}
public:
	static StatusInfo Clear()
	{
		return StatusInfo(std::nullopt, std::nullopt);
	}
	static StatusInfo Message(QString message)
	{
		return StatusInfo(message, std::nullopt);
	}
	static StatusInfo Progress(QString message, float progress = -1.0f)
	{
		return StatusInfo(message, progress);
	}
	static StatusInfo Progress(float progress)
	{
		return StatusInfo(std::nullopt, progress);
	}
};
Q_DECLARE_METATYPE(StatusInfo)

#endif // STATUSINFO_H
