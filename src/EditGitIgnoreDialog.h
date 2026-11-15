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
	
	QString working_dir_;
	// QString file_;
	QStringList files_;
	std::map<QRadioButton *, QString> text_map_;
		
	void updateComboBox();
	void updateRadioButtons();
public:
	explicit EditGitIgnoreDialog(MainWindow *parent, QStringList const &file);
	~EditGitIgnoreDialog() override;
	
	QStringList files() const;

	QString gitignorePath() const;
private slots:
	void on_pushButton_edit_file_clicked();

	void on_comboBox_file_currentTextChanged(const QString &arg1);
};

#endif // EDITGITIGNOREDIALOG_H
