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
	struct Private;
	Private *m;
public:
	explicit GenerateCommitMessageDialog(QWidget *parent = nullptr);
	~GenerateCommitMessageDialog();
	QString text() const;
	void generate();
private slots:
	void on_pushButton_regenerate_clicked();
	void onReady(QStringList const &list);
signals:
	void ready(QStringList const &list);
};

#endif // GENERATECOMMITMESSAGEDIALOG_H
