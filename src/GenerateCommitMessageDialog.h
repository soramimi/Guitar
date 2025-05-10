#ifndef GENERATECOMMITMESSAGEDIALOG_H
#define GENERATECOMMITMESSAGEDIALOG_H

#include <QDialog>
#include <QObject>

namespace Ui {
class GenerateCommitMessageDialog;
}

class QListWidgetItem;
class GeneratedCommitMessage;

class GenerateCommitMessageDialog : public QDialog {
	Q_OBJECT
private:
	Ui::GenerateCommitMessageDialog *ui;
	struct Private;
	Private *m;
public:
	explicit GenerateCommitMessageDialog(QWidget *parent, std::string const &model_name);
	~GenerateCommitMessageDialog();
	void generate(const std::string &diff);
	std::string diffText() const;
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
