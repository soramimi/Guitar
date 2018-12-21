#ifndef EDITREMOTEDIALOG_H
#define EDITREMOTEDIALOG_H

#include <QDialog>

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
public:
	explicit EditRemoteDialog(BasicMainWindow *parent, Operation op);
	~EditRemoteDialog();

	void setName(QString const &s) const;
	void setUrl(QString const &s) const;

	QString name() const;
	QString url() const;
	int exec();
private slots:
	void on_pushButton_test_clicked();

private:
	Ui::EditRemoteDialog *ui;
	BasicMainWindow *mainwindow();
};

#endif // EDITREMOTEDIALOG_H
