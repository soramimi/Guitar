#ifndef GENERATECOMMITMESSAGEDIALOG_H
#define GENERATECOMMITMESSAGEDIALOG_H

#include "CommitMessageGenerator.h"
#include <QDialog>
#include <QObject>

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
	void generate(QString const &diff);
	QString diffText() const;
	QString message() const;
private slots:
	void on_pushButton_regenerate_clicked();
	void onReady(GeneratedCommitMessage const &list);
};

#endif // GENERATECOMMITMESSAGEDIALOG_H
