#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "../MeCaSearch.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT
private:
	Ui::MainWindow *ui;
	MeCaSearch mecaSearch_;
public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

};
#endif // MAINWINDOW_H
