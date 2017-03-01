#include "CloneDialog.h"
#include "ui_CloneDialog.h"
#include "misc.h"
#include "joinpath.h"

#include <QMessageBox>
#include <QThread>

CloneDialog::CloneDialog(QWidget *parent, GitPtr gitptr, const QString &defworkdir) :
	QDialog(parent),
	ui(new Ui::CloneDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	git = gitptr;
	default_working_dir = defworkdir;
	ui->lineEdit_working_dir->setText(default_working_dir);
}

CloneDialog::~CloneDialog()
{
	delete ui;
}

QString CloneDialog::workingDir() const
{
	return working_dir;
}

class CloneThread : public QThread {
protected:
	CloneDialog *caller;
	GitPtr g;
	QString url;
	QString into;
	virtual void run()
	{
		ok = g->clone(url, into, callback_, caller);
	}
public:
	bool ok = false;
	CloneThread(GitPtr g, QString const &url, QString const &into)
		: g(g)
		, url(url)
		, into(into)
	{
	}
private:
	static void callback_(void *cookie, const char *ptr, int len)
	{
		CloneDialog *me = (CloneDialog *)cookie;

		qDebug() << QString::fromUtf8(ptr, len);
	}
};



void CloneDialog::accept()
{
	QString url = ui->lineEdit_repo_location->text();
	working_dir = ui->lineEdit_working_dir->text();

	{
		OverrideWaitCursor;
//		if (git->clone(url, working_dir, callback_, this)) {
//			QDialog::accept();
//			return;
//		}
		CloneThread th(git, url, working_dir);
		th.start();
		th.wait();
		if (th.ok) {
			QDialog::accept();
			return;
		}
	}

	QString errmsg = git->errorMessage();
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
	path = default_working_dir / path;
	path = misc::normalizePathSeparator(path);
	ui->lineEdit_working_dir->setText(path);
}
