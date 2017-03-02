#include "CloneDialog.h"
#include "ui_CloneDialog.h"
#include "misc.h"
#include "joinpath.h"
#include "MainWindow.h"

#include <QMessageBox>
#include <QThread>

class CloneThread : public QThread {
protected:
	CloneDialog *caller;
	GitPtr g;
	QString url;
	QString into;
	virtual void run()
	{
		caller->ok = g->clone(url, into, callback_, caller);
		if (!caller->ok) {
			caller->errmsg = g->errorMessage();
		}
		emit caller->done();
	}
public:
	CloneThread(CloneDialog *caller, GitPtr g, QString const &url, QString const &into)
		: caller(caller)
		, g(g)
		, url(url)
		, into(into)
	{
	}
private:
	static void callback_(void *cookie, const char *ptr, int len)
	{
		CloneDialog *me = (CloneDialog *)cookie;
		QByteArray ba(ptr, len);
//		emit me->writeLog(ba);
		emit me->mainwindow()->writeLog(ba);
	}
};


struct CloneDialog::Private {
	MainWindow *mainwindow;
	GitPtr git;
	QString default_working_dir;
	QString working_dir;
	std::shared_ptr<CloneThread> thread;
};

CloneDialog::CloneDialog(QWidget *parent, GitPtr gitptr, const QString &defworkdir) :
	QDialog(parent),
	ui(new Ui::CloneDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	pv = new Private();

	pv->mainwindow = qobject_cast<MainWindow *>(parent);

	pv->git = gitptr;
	pv->default_working_dir = defworkdir;
	ui->lineEdit_working_dir->setText(pv->default_working_dir);

	connect(this, SIGNAL(done()), this, SLOT(onDone()));
}

CloneDialog::~CloneDialog()
{
	delete pv;
	delete ui;
}

MainWindow *CloneDialog::mainwindow()
{
	return pv->mainwindow;
}

QString CloneDialog::workingDir() const
{
	return pv->working_dir;
}

void CloneDialog::accept()
{
	setEnabled(false);

	QString url = ui->lineEdit_repo_location->text();
	pv->working_dir = ui->lineEdit_working_dir->text();

	qApp->setOverrideCursor(Qt::WaitCursor);

	pv->thread = std::shared_ptr<CloneThread>(new CloneThread(this, pv->git, url, pv->working_dir));
	pv->thread->start();
}

void CloneDialog::reject()
{
	if (isEnabled()) {     // when dialog is enabled
		QDialog::reject(); // close dialog
	}
}

void CloneDialog::onDone()
{
	qApp->restoreOverrideCursor();
	setEnabled(true);

	pv->thread.reset();

	if (ok) {
		QDialog::accept();
		return;
	}

	errmsg = pv->git->errorMessage();
	QMessageBox::warning(this, qApp->applicationName(), errmsg);
}

void CloneDialog::on_lineEdit_repo_location_textChanged(const QString &text)
{
	QString path;
	int i = text.lastIndexOf('/');
	int j = text.lastIndexOf('\\');
	if (i < j) i = j;
	j = text.size();
	if (text.endsWith(".git")) {
		j -= 4;
	}
	if (i >= 0 && i < j) {
		path = text.mid(i, j - i);
	}
	path = pv->default_working_dir / path;
	path = misc::normalizePathSeparator(path);
	ui->lineEdit_working_dir->setText(path);
}






