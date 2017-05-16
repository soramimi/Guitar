#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog
{
	Q_OBJECT
private:
	Ui::AboutDialog *ui;
	QPixmap pixmap;
public:
	explicit AboutDialog(QWidget *parent = 0);
	~AboutDialog();

	static QString appVersion();
protected:
	void mouseReleaseEvent(QMouseEvent *);
protected:
	void paintEvent(QPaintEvent *event);
};

#endif // ABOUTDIALOG_H
