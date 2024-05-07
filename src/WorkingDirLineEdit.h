#ifndef WORKINGDIRLINEEDIT_H
#define WORKINGDIRLINEEDIT_H

#include <QLineEdit>
#include <QWidget>

class WorkingDirLineEdit : public QLineEdit {
	Q_OBJECT
public:
	explicit WorkingDirLineEdit(QWidget *parent = nullptr);
protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;
};

#endif // WORKINGDIRLINEEDIT_H
