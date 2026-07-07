#include "CommitViewWindow.h"
#include "ui_CommitViewWindow.h"
#include "ApplicationGlobal.h"
#include <common/q/helper.h>
#include <QMenu>
#include "MainWindow.h"

struct CommitViewWindow::Private {
	GitCommitItem const *commit = nullptr;
	// std::vector<GitDiff> diff_list.list_;
	MainWindow::DiffResult diff_list;
};

MainWindow *CommitViewWindow::mainwindow()
{
	return global->mainwindow;
}

CommitViewWindow::CommitViewWindow(MainWindow *parent, GitCommitItem const *commit)
	: QDialog(parent)
	, m(new Private)
	, ui(new Ui::CommitViewWindow)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	m->commit = commit;

	ui->widget_diff->init();

	ui->lineEdit_message->setText((QS)m->commit->message);
	ui->lineEdit_id->setText(QString::fromStdString(m->commit->commit_id.toString()));

	m->diff_list = mainwindow()->makeDiffList(m->commit->commit_id, ui->listWidget_files);

	ui->listWidget_files->setCurrentRow(0);
}

CommitViewWindow::~CommitViewWindow()
{
	delete m;
	delete ui;
}

void CommitViewWindow::on_listWidget_files_currentRowChanged(int currentRow)
{
	std::basic_string_view<GitDiff const *> items = m->diff_list.items();
	if (currentRow >= 0 && (size_t)currentRow < items.size()) {
		GitDiff const *diff = items[currentRow];
		ui->widget_diff->updateDiffView(*diff, false, FileDiffWidget::SideBySide);
	}
}

void CommitViewWindow::on_listWidget_files_customContextMenuRequested(const QPoint &pos)
{
	if (!mainwindow()->git().isValidWorkingCopy()) return;

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
			mainwindow()->showObjectProperty(item);
		}
	}
}
