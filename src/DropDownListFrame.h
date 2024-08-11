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
	void addItem(QString const &text) const;
	void setItems(QStringList const &list) const;
	void show_();
protected:
	bool eventFilter(QObject *watched, QEvent *event);
	void focusOutEvent(QFocusEvent *event);
	void keyPressEvent(QKeyEvent *event);
public slots:
	void setVisible(bool visible);
signals:
	void itemClicked(QString const &text);
	void done();
};

#endif // DROPDOWNLISTFRAME_H
