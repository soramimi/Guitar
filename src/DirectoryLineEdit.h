#ifndef DIRECTORYLINEEDIT_H
#define DIRECTORYLINEEDIT_H

#include <QLineEdit>
#include <QWidget>

class DirectoryLineEdit : public QLineEdit
{
	Q_OBJECT
public:
	explicit DirectoryLineEdit(QWidget *parent = 0);

signals:

public slots:
protected:
	void dragEnterEvent(QDragEnterEvent *event);

	// QWidget interface
protected:
	void dropEvent(QDropEvent *event);
};

#endif // DIRECTORYLINEEDIT_H
