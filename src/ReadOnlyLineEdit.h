#ifndef READONLYLINEEDIT_H
#define READONLYLINEEDIT_H

#include <QLineEdit>

class ReadOnlyLineEdit : public QLineEdit {
public:
	ReadOnlyLineEdit(QWidget *parent = nullptr);
	void setText(QString const &text);
protected:
	void paintEvent(QPaintEvent *event) override;
};

#endif // READONLYLINEEDIT_H
