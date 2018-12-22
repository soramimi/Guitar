#ifndef DIRECTORYLINEEDIT_H
#define DIRECTORYLINEEDIT_H

#include <QLineEdit>
#include <QWidget>

class DirectoryLineEdit : public QLineEdit {
	Q_OBJECT
public:
	explicit DirectoryLineEdit(QWidget *parent = nullptr);
protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;
};

#endif // DIRECTORYLINEEDIT_H
