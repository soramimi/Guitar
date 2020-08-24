#ifndef FILEPROPERTYDIALOG_H
#define FILEPROPERTYDIALOG_H

#include <QDialog>

namespace Ui {
class FilePropertyDialog;
}

class MainWindow;

class FilePropertyDialog : public QDialog {
	Q_OBJECT
private:
	MainWindow *mainwindow;
public:
	explicit FilePropertyDialog(QWidget *parent = nullptr);
	~FilePropertyDialog() override;

	void exec(MainWindow *mw, QString const &path, QString const &id);
private:
	Ui::FilePropertyDialog *ui;
};

#endif // FILEPROPERTYDIALOG_H
