#ifndef FILEPROPERTYDIALOG_H
#define FILEPROPERTYDIALOG_H

#include <QDialog>

namespace Ui {
class FilePropertyDialog;
}

class BasicMainWindow;

class FilePropertyDialog : public QDialog {
	Q_OBJECT
private:
	BasicMainWindow *mainwindow;
public:
	explicit FilePropertyDialog(QWidget *parent = 0);
	~FilePropertyDialog();

	void exec(BasicMainWindow *mw, QString const &path, QString const &id);
private:
	Ui::FilePropertyDialog *ui;
};

#endif // FILEPROPERTYDIALOG_H
