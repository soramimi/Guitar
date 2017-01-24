#ifndef FILEPROPERTYDIALOG_H
#define FILEPROPERTYDIALOG_H

#include <QDialog>

namespace Ui {
class FilePropertyDialog;
}

class MainWindow;

class FilePropertyDialog : public QDialog
{
	Q_OBJECT
private:
	MainWindow *mainwindow;
public:
	explicit FilePropertyDialog(QWidget *parent = 0);
	~FilePropertyDialog();

	void exec(MainWindow *mw, const QString &path, const QString &id);
private:
	Ui::FilePropertyDialog *ui;
};

#endif // FILEPROPERTYDIALOG_H
