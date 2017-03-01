#ifndef CLONEDIALOG_H
#define CLONEDIALOG_H

#include <QDialog>
#include "Git.h"

namespace Ui {
class CloneDialog;
}

class MainWindow;

class CloneDialog : public QDialog
{
	Q_OBJECT
	friend class CloneThread;
private:
	struct Private;
	Private *pv;

	bool ok = false;
	QString errmsg;

	typedef std::shared_ptr<Git> GitPtr;
public:
	explicit CloneDialog(QWidget *parent, GitPtr gitptr, QString const &defworkdir);
	~CloneDialog();

	QString workingDir() const;
private:
	Ui::CloneDialog *ui;

	MainWindow *mainwindow();
public slots:
	void accept();
	void reject();
	void onDone();
private slots:
	void on_lineEdit_repo_location_textChanged(const QString &arg1);
signals:
	void done();
};

#endif // CLONEDIALOG_H
