#include "CommitViewWindow.h"
#include "ui_CommitViewWindow.h"

#include <QMenu>

struct CommitViewWindow::Private {
	Git::CommitItem const *commit = nullptr;
	QList<Git::Diff> diff_list;
};

CommitViewWindow::CommitViewWindow(BasicMainWindow *parent, Git::CommitItem const *commit)
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

BasicMainWindow *CommitViewWindow::mainwindow()
{
	return qobject_cast<BasicMainWindow *>(parent());
}

void CommitViewWindow::on_listWidget_files_currentRowChanged(int currentRow)
{
	if (currentRow >= 0 && currentRow < m->diff_list.size()) {
		Git::Diff const &diff = m->diff_list[currentRow];
		ui->widget_diff->updateDiffView(diff, false);
	}
}

void CommitViewWindow::on_listWidget_files_customContextMenuRequested(const QPoint &pos)
{
	GitPtr g = mainwindow()->git();
	if (!mainwindow()->isValidWorkingCopy(g)) return;

	QMenu menu;
	QAction *a_history = menu.addAction(tr("History"));
	QAction *a_properties = mainwindow()->addMenuActionProperty(&menu);

	QPoint pt = ui->listWidget_files->mapToGlobal(pos) + QPoint(8, -8);
	QAction *a = menu.exec(pt);
	if (a) {
		QListWidgetItem *item = ui->listWidget_files->currentItem();
		if (a == a_history) {
			mainwindow()->execFileHistory(item);
		} else if (a == a_properties) {
			mainwindow()->execFilePropertyDialog(item);
		}
	}
}
