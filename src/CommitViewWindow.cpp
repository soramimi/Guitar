#include "CommitViewWindow.h"
#include "ui_CommitViewWindow.h"

struct CommitViewWindow::Private {
	Git::CommitItem const *commit = nullptr;
	QList<Git::Diff> diff_list;
};

CommitViewWindow::CommitViewWindow(MainWindow *parent, const Git::CommitItem *commit)
	: QDialog(parent)
	, ui(new Ui::CommitViewWindow)
	, m(new Private)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->commit = commit;

	ui->widget_diff->bind(mainwindow());

	ui->lineEdit_message->setText(m->commit->message);
	ui->lineEdit_id->setText(m->commit->commit_id);

	mainwindow()->updateFilesList(m->commit->commit_id, &m->diff_list, ui->listWidget_files);

	ui->listWidget_files->setCurrentRow(0);
}

CommitViewWindow::~CommitViewWindow()
{
	delete m;
	delete ui;
}

MainWindow *CommitViewWindow::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}



void CommitViewWindow::on_listWidget_files_currentRowChanged(int currentRow)
{
	if (currentRow >= 0 && currentRow < m->diff_list.size()) {
		Git::Diff const &diff = m->diff_list[currentRow];
		ui->widget_diff->updateDiffView(diff, false);
	}
}
