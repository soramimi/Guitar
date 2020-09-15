#ifndef SUBMODULEMAINWINDOW_H
#define SUBMODULEMAINWINDOW_H

#include <QMainWindow>
#include "Git.h"

class MainWindow;
class RepositoryWrapperFrame;

namespace Ui {
class SubmoduleMainWindow;
}

class SubmoduleMainWindow : public QMainWindow {
	Q_OBJECT
private:
	Ui::SubmoduleMainWindow *ui;
	MainWindow *mw_;
	GitPtr g_;
	MainWindow *mainwindow();
	GitPtr git();
public:
	explicit SubmoduleMainWindow(MainWindow *parent, GitPtr g);
	~SubmoduleMainWindow();

	RepositoryWrapperFrame *frame();

	void reset();
};

#endif // SUBMODULEMAINWINDOW_H
