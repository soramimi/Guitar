#ifndef MYOBJECTVIEWBASE_H
#define MYOBJECTVIEWBASE_H

#include <QObject>
#include <QString>

class MyObjectViewBase {
protected:
	QString object_id_;
	QString object_path_;
	bool isValidObject() const;
	bool saveAs(QWidget *parent);
public:
};

#endif // MYOBJECTVIEWBASE_H
