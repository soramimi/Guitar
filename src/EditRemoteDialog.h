#ifndef EDITREMOTEDIALOG_H
#define EDITREMOTEDIALOG_H

#include <QDialog>
#include "Git.h"

class BasicMainWindow;

namespace Ui {
class EditRemoteDialog;
}

class EditRemoteDialog : public QDialog {
	Q_OBJECT
public:
	enum Operation {
		RemoteAdd,
		RemoteSet,
	};
private:
	Ui::EditRemoteDialog *ui;
	BasicMainWindow *mainwindow();
public:
	explicit EditRemoteDialog(BasicMainWindow *parent, Operation op, const Git::Context *gcx);
	~EditRemoteDialog() override;

	void setName(QString const &s) const;
	void setUrl(QString const &s) const;
	void setSshKey(const QString &s) const;

	QString name() const;
	QString url() const;
	QString sshKey() const;
	int exec() override;
private slots:
	void on_pushButton_test_clicked();
};

#endif // EDITREMOTEDIALOG_H
