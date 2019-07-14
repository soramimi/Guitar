#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog {
	Q_OBJECT
private:
	Ui::AboutDialog *ui;
	QPixmap pixmap;
public:
	explicit AboutDialog(QWidget *parent = nullptr);
	~AboutDialog() override;

	static QString appVersion();
protected:
	void mouseReleaseEvent(QMouseEvent *) override;
	void paintEvent(QPaintEvent *event) override;
};

#endif // ABOUTDIALOG_H
