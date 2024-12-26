#ifndef PUSHDIALOG_H
#define PUSHDIALOG_H

#include <QDialog>

namespace Ui {
class PushDialog;
}

class PushDialog : public QDialog {
	Q_OBJECT
private:
	Ui::PushDialog *ui;
	void updateUI();
public:
	struct RemoteBranch {
		QString remote;
		QString branch;
		RemoteBranch() = default;
		RemoteBranch(QString const &r, QString const &b)
			: remote(r)
			, branch(b)
		{
		}
	};
public:
	explicit PushDialog(QWidget *parent, QString const &url, QStringList const &remotes, QStringList const &branches, RemoteBranch const &remote_branch);
	~PushDialog() override;
	bool isSetUpStream() const;
	QString remote() const;
	QString branch() const;
	bool isForce() const;
private slots:
	void on_checkBox_force_clicked();
	void on_checkBox_really_force_clicked();
};

#endif // PUSHDIALOG_H
