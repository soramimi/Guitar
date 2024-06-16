#ifndef GENERATECOMMITMESSAGEDIALOG_H
#define GENERATECOMMITMESSAGEDIALOG_H

#include "CommitMessageGenerator.h"

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
	explicit GenerateCommitMessageDialog(QWidget *parent, const QString &model_name);
	~GenerateCommitMessageDialog();
	QString text() const;
	void generate();
private slots:
	void on_pushButton_regenerate_clicked();
	void onReady(GeneratedCommitMessage const &list);
signals:
	void ready(GeneratedCommitMessage const &result);
};

#endif // GENERATECOMMITMESSAGEDIALOG_H
