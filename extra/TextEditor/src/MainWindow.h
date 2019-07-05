#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "texteditor/InputMethodPopup.h"

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

private:
	struct Private;
	Private *m;
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

private:
	Ui::MainWindow *ui;

	void upArrow();
	void downArrow();
	void leftArrow();
	void rightArrow();

	TextEditorEnginePtr engine();
	Document *document();
protected:
	void keyPressEvent(QKeyEvent *);

public:
	bool event(QEvent *);

private slots:
	void on_verticalScrollBar_valueChanged(int value);
	void on_horizontalScrollBar_valueChanged(int value);
	void on_action_file_open_triggered();
	void on_action_file_save_triggered();
	void on_action_test_triggered();
	void updateIm();
protected:
	void moveEvent(QMoveEvent *);
signals:
	void hoge();
};

#endif // MAINWINDOW_H
