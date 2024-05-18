#ifndef GENERATECOMMITMESSAGEDIALOG_H
#define GENERATECOMMITMESSAGEDIALOG_H

#include <QDialog>

namespace Ui {
class GenerateCommitMessageWithAiDialog;
}

class GenerateCommitMessageDialog : public QDialog {
	Q_OBJECT
private:
	Ui::GenerateCommitMessageWithAiDialog *ui;
public:
	explicit GenerateCommitMessageDialog(QWidget *parent = nullptr);
	~GenerateCommitMessageDialog();
	QString text() const;
	void generate();
};

#endif // GENERATECOMMITMESSAGEDIALOG_H
