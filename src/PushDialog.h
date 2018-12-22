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
	explicit PushDialog(QWidget *parent, QStringList const &remotes, QStringList const &branches, RemoteBranch const &remote_branch);
	~PushDialog() override;

	enum Action {
		PushSimple,
		PushSetUpstream,
	};

	Action action() const;
	QString remote() const;
	QString branch() const;
};

#endif // PUSHDIALOG_H
