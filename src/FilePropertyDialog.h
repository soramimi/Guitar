#ifndef FILEPROPERTYDIALOG_H
#define FILEPROPERTYDIALOG_H

#include "Git.h"

#include <QDialog>

namespace Ui {
class FilePropertyDialog;
}

class MainWindow;

class FilePropertyDialog : public QDialog {
	Q_OBJECT
private:
	// MainWindow *mainwindow;
public:
	explicit FilePropertyDialog(QWidget *parent = nullptr);
	~FilePropertyDialog() override;

	void exec(QString const &path, const GitHash &id);
private:
	Ui::FilePropertyDialog *ui;
};

#endif // FILEPROPERTYDIALOG_H
