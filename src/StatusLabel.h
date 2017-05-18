#ifndef STATUSLABEL_H
#define STATUSLABEL_H

#include <QLabel>

class StatusLabel : public QLabel
{
	Q_OBJECT
public:
	explicit StatusLabel(QWidget *parent = 0);
	QSize minimumSizeHint() const;
};

#endif // STATUSLABEL_H
