#ifndef GENERATECOMMITMESSAGEDIALOG_H
#define GENERATECOMMITMESSAGEDIALOG_H

#include "CommitMessageGenerator.h"
#include <QDialog>
#include <QObject>

namespace Ui {
class GenerateCommitMessageDialog;
}

class QListWidgetItem;

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
	QStringList message() const;
private slots:
	void on_pushButton_regenerate_clicked();
	void onReady(GeneratedCommitMessage const &list);
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

	// QDialog interface
public slots:
	void done(int stat);
};

#endif // GENERATECOMMITMESSAGEDIALOG_H
