#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "LogView.h"
#include <optional>
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();
protected:
	void closeEvent(QCloseEvent *event);
private:
	Ui::MainWindow *ui;
	void startReceiveThread();
	void closesocket();
	void installCustomLogger();
private slots:
	void onLogReady();
	void on_action_send_test_message_triggered();

	void on_action_clear_triggered();

signals:
	void logReady();
};

#endif // MAINWINDOW_H
