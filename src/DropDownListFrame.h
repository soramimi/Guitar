#ifndef DROPDOWNLISTFRAME_H
#define DROPDOWNLISTFRAME_H

#include <QFrame>

class QListWidget;

class DropDownListFrame : public QFrame {
	Q_OBJECT
private:
public:
	QListWidget *listw_;
	DropDownListFrame(QWidget *parent);
	void addItem(QString const &text);
	void setItems(QStringList const &list);
	void show();
protected:
	bool eventFilter(QObject *watched, QEvent *event);
	void focusOutEvent(QFocusEvent *event);
	void keyPressEvent(QKeyEvent *event);
signals:
	void itemClicked(QString const &text);
};

#endif // DROPDOWNLISTFRAME_H
