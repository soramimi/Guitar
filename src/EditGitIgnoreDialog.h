#ifndef EDITGITIGNOREDIALOG_H
#define EDITGITIGNOREDIALOG_H

#include <QDialog>

class MainWindow;

namespace Ui {
class EditGitIgnoreDialog;
}

class EditGitIgnoreDialog : public QDialog
{
	Q_OBJECT
private:
	QString gitignore_path;
public:
	explicit EditGitIgnoreDialog(MainWindow *parent, QString gitignore_path, QString const &file);
	~EditGitIgnoreDialog();

	QString text() const;

private slots:
	void on_pushButton_edit_file_clicked();

private:
	Ui::EditGitIgnoreDialog *ui;
	MainWindow *mainwindow();
};

#endif // EDITGITIGNOREDIALOG_H
