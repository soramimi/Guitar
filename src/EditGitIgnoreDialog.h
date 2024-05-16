#ifndef EDITGITIGNOREDIALOG_H
#define EDITGITIGNOREDIALOG_H

#include <QDialog>
#include <map>

class MainWindow;
class QRadioButton;

namespace Ui {
class EditGitIgnoreDialog;
}

class EditGitIgnoreDialog : public QDialog {
	Q_OBJECT
private:
	Ui::EditGitIgnoreDialog *ui;
	MainWindow *mainwindow();
	
	QString gitignore_path;
	std::map<QRadioButton *, QString> text_map_;
		
public:
	explicit EditGitIgnoreDialog(MainWindow *parent, QString const &gitignore_path, QString const &file);
	~EditGitIgnoreDialog() override;

	QString text() const;

private slots:
	void on_pushButton_edit_file_clicked();

};

#endif // EDITGITIGNOREDIALOG_H
