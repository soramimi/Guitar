#ifndef GENERATECOMMITMESSAGEDIALOG_H
#define GENERATECOMMITMESSAGEDIALOG_H

#include "CommitMessageGenerator.h"
#include "GenerativeAI.h"
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
	const GenerativeAI::Model &ai_model() const;
	void init_ai_models(const std::vector<GenerativeAI::Model> &models, int default_index);
	void _generate(const std::string &diff, const std::string &status_s);
public:
	explicit GenerateCommitMessageDialog(QWidget *parent, std::vector<GenerativeAI::Model> const &models, int default_index);
	~GenerateCommitMessageDialog();
	void generate(std::string const &id_a, std::string const &id_b);
	void generate();
	std::string diffText() const;
	QStringList message() const;
	void setCommitIDs(CommitMessageGenerator::CommitPair const &commits);
private slots:
	void on_pushButton_regenerate_clicked();
	void onReady(GeneratedCommitMessage const &list);
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);
	void on_checkBox_hint_checkStateChanged(const Qt::CheckState &arg1);

public slots:
	void done(int stat);
};

#endif // GENERATECOMMITMESSAGEDIALOG_H
