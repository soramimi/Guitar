#include "SubmoduleMainWindow.h"
#include "ui_SubmoduleMainWindow.h"

SubmoduleMainWindow::SubmoduleMainWindow(MainWindow *parent)
	: QMainWindow(parent)
	, ui(new Ui::SubmoduleMainWindow)
{
	ui->setupUi(this);
	mw_ = parent;
	ui->frame_repository_wrapper->bind(mw_
									   , ui->tableWidget_log
									   , ui->listWidget_files
									   , ui->listWidget_unstaged
									   , ui->listWidget_staged
									   , ui->widget_diff_view
									   );
	ui->frame_repository_wrapper->prepareLogTableWidget();
}

SubmoduleMainWindow::~SubmoduleMainWindow()
{
	delete ui;
}

MainWindow *SubmoduleMainWindow::mainwindow()
{
	return mw_;
}

RepositoryWrapperFrame *SubmoduleMainWindow::frame()
{
	return ui->frame_repository_wrapper;
}

void SubmoduleMainWindow::reset()
{
	GitPtr g = mainwindow()->git();
	mainwindow()->openRepository_(ui->frame_repository_wrapper, g);

}
