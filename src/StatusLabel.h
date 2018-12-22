#ifndef STATUSLABEL_H
#define STATUSLABEL_H

#include <QLabel>

class StatusLabel : public QLabel {
	Q_OBJECT
public:
	explicit StatusLabel(QWidget *parent = nullptr);
	QSize minimumSizeHint() const override;
};

#endif // STATUSLABEL_H
