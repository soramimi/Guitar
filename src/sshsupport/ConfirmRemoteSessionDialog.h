#ifndef CONFIRMREMOTESESSIONDIALOG_H
#define CONFIRMREMOTESESSIONDIALOG_H

#include <QDialog>

namespace Ui {
class ConfirmRemoteSessionDialog;
}

class ConfirmRemoteSessionDialog : public QDialog {
	Q_OBJECT
public:
	explicit ConfirmRemoteSessionDialog(QWidget *parent = nullptr);
	~ConfirmRemoteSessionDialog();
	
	int exec(QString const &remote_host, QString const &remote_command);
	bool yes() const;

private slots:
	void on_checkBox_remote_host_checkStateChanged(const Qt::CheckState &arg1);

	void on_checkBox_remote_command_checkStateChanged(const Qt::CheckState &arg1);

private:
	Ui::ConfirmRemoteSessionDialog *ui;
};

#endif // CONFIRMREMOTESESSIONDIALOG_H
