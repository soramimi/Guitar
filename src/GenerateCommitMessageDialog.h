#ifndef GENERATECOMMITMESSAGEDIALOG_H
#define GENERATECOMMITMESSAGEDIALOG_H

#include <QDialog>

namespace Ui {
class GenerateCommitMessageDialog;
}

class GenerateCommitMessageDialog : public QDialog {
	Q_OBJECT
private:
	Ui::GenerateCommitMessageDialog *ui;
public:
	explicit GenerateCommitMessageDialog(QWidget *parent = nullptr);
	~GenerateCommitMessageDialog();
	QString text() const;
	void generate();
private slots:
	void on_pushButton_regenerate_clicked();
};

#endif // GENERATECOMMITMESSAGEDIALOG_H
