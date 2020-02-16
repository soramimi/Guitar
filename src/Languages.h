#ifndef LANGUAGES_H
#define LANGUAGES_H

#include <QObject>

class Languages : public QObject {
	Q_OBJECT
public:
	struct Item {
		QString id;
		QString description;
		Item() = default;
		Item(QString const &id, QString const &description)
			: id(id)
			, description(description)
		{
		}
	};
	QList<Item> items;

	explicit Languages(QObject *parent = nullptr);
	void addLanguage(QString const &code, QString const &description)
	{
		items.push_back(Item(code, description));
	}
};

#endif // LANGUAGES_H
