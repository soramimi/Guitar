#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog
{
	Q_OBJECT

public:
	explicit AboutDialog(QWidget *parent = 0);
	~AboutDialog();

	static QString appVersion();
private:
	Ui::AboutDialog *ui;

	// QWidget interface
protected:
	void mouseReleaseEvent(QMouseEvent *);
};

#endif // ABOUTDIALOG_H
