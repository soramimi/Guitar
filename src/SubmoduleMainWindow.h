#ifndef SUBMODULEMAINWINDOW_H
#define SUBMODULEMAINWINDOW_H

#include <QMainWindow>

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
	MainWindow *mainwindow();
public:
	explicit SubmoduleMainWindow(MainWindow *parent = nullptr);
	~SubmoduleMainWindow();

	RepositoryWrapperFrame *frame();

	void reset();
};

#endif // SUBMODULEMAINWINDOW_H
