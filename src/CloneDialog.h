#ifndef CLONEDIALOG_H
#define CLONEDIALOG_H

#include <QDialog>
#include <QThread>
#include "Git.h"

namespace Ui {
class CloneDialog;
}

class MainWindow;

class CloneThread : public QThread {
protected:
//	CloneDialog *caller;
	GitPtr g;
	QString url;
	QString into;
	bool ok = false;
	QString errmsg;
	virtual void run()
	{

		ok = g->clone(Git::preclone(url, into));
		if (!ok) {
			errmsg = g->errorMessage();
		}
//		emit caller->done();
	}
public:
	CloneThread(GitPtr g, QString const &url, QString const &into)
		: g(g)
		, url(url)
		, into(into)
	{
	}
};

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
	explicit CloneDialog(QWidget *parent, QString const &url, QString const &defworkdir);
	~CloneDialog();

	QString url();
	QString dir();
private:
	Ui::CloneDialog *ui;

	MainWindow *mainwindow();
private slots:
	void on_lineEdit_repo_location_textChanged(const QString &arg1);
	void on_pushButton_test_clicked();
};

#endif // CLONEDIALOG_H
