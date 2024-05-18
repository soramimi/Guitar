#ifndef CHOICECOMMITMESSAGEDIALOG_H
#define CHOICECOMMITMESSAGEDIALOG_H

#include <QDialog>

namespace Ui {
class GenerateCommitMessageWithAiDialog;
}

class ChoiceCommitMessageDialog : public QDialog {
	Q_OBJECT
private:
	Ui::GenerateCommitMessageWithAiDialog *ui;
public:
	explicit ChoiceCommitMessageDialog(const QStringList &list, QWidget *parent = nullptr);
	~ChoiceCommitMessageDialog();
	QString text() const;
};

#endif // CHOICECOMMITMESSAGEDIALOG_H
