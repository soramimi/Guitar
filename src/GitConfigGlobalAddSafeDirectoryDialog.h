#ifndef GITCONFIGGLOBALADDSAFEDIRECTORYDIALOG_H
#define GITCONFIGGLOBALADDSAFEDIRECTORYDIALOG_H

#include <QDialog>

namespace Ui {
class GitConfigGlobalAddSafeDirectoryDialog;
}

class GitConfigGlobalAddSafeDirectoryDialog : public QDialog {
	Q_OBJECT
private:
	Ui::GitConfigGlobalAddSafeDirectoryDialog *ui;
public:
	explicit GitConfigGlobalAddSafeDirectoryDialog(QWidget *parent = nullptr);
	~GitConfigGlobalAddSafeDirectoryDialog();
	void setMessage(QString const &message, const QString &command);
private slots:
	void on_checkBox_confirm_checkStateChanged(const Qt::CheckState &arg1);
};

#endif // GITCONFIGGLOBALADDSAFEDIRECTORYDIALOG_H
