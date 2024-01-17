#include "SubmoduleMainWindow.h"
#include "ui_SubmoduleMainWindow.h"

SubmoduleMainWindow::SubmoduleMainWindow(MainWindow *parent, GitPtr g)
	: QMainWindow(parent)
	, ui(new Ui::SubmoduleMainWindow)
{
	ui->setupUi(this);
	mw_ = parent;
	g_ = g;
	ui->frame_repository_wrapper2->bind(mw_
									   , ui->tableWidget_log2
									   , ui->listWidget_files
									   , ui->listWidget_unstaged
									   , ui->listWidget_staged
									   , ui->widget_diff_view
									   );
	ui->frame_repository_wrapper2->prepareLogTableWidget();

	QString text = g->workingDir();
	text = "Submodule " + text;
	setWindowTitle(text);

	ui->stackedWidget_filelist->setCurrentWidget(ui->page_files);
}

SubmoduleMainWindow::~SubmoduleMainWindow()
{
	delete ui;
}

MainWindow *SubmoduleMainWindow::mainwindow()
{
	return mw_;
}

GitPtr SubmoduleMainWindow::git()
{
	return g_->dup();
}

RepositoryWrapperFrame *SubmoduleMainWindow::frame()
{
	return ui->frame_repository_wrapper2;
}

void SubmoduleMainWindow::reset()
{
	GitPtr g = git();
	mainwindow()->openRepositoryWithFrame(ui->frame_repository_wrapper2, g);

}
