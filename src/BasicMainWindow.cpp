#include "BasicMainWindow.h"

BasicMainWindow::BasicMainWindow(QWidget *parent)
	: QMainWindow(parent)
{
}

WebContext *BasicMainWindow::webContext()
{
	return &webcx_;
}
