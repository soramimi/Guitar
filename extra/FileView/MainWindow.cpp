#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	ui->widget->bind(this, nullptr, ui->verticalScrollBar, ui->horizontalScrollBar);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_action_open_text_triggered()
{
	QByteArray ba;
	QString path = QFileDialog::getOpenFileName(this);
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		ba = file.readAll();
	}
	ui->widget->setText(ba, this, QString(), path);
}

void MainWindow::on_action_open_image_triggered()
{
	QByteArray ba;
	QString path = QFileDialog::getOpenFileName(this);
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		ba = file.readAll();
	}
	ui->widget->setImage(QString(), ba, QString(), path);
}

void MainWindow::on_horizontalScrollBar_valueChanged(int value)
{
	ui->widget->refrectScrollBar();
}

void MainWindow::on_verticalScrollBar_valueChanged(int value)
{
	ui->widget->refrectScrollBar();
}
