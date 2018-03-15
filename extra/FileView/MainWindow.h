#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "texteditor/TextEditorTheme.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT
private:
	TextEditorThemePtr texteditor_theme_;
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

private slots:
	void on_action_open_text_triggered();

	void on_action_open_image_triggered();

	void on_horizontalScrollBar_valueChanged(int value);

	void on_verticalScrollBar_valueChanged(int value);

private:
	Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
