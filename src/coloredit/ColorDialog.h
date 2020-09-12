#ifndef COLORDIALOG_H
#define COLORDIALOG_H

#include <QDialog>

namespace Ui {
class ColorDialog;
}

class ColorDialog : public QDialog {
	Q_OBJECT
private:
public:
	explicit ColorDialog(QWidget *parent, QColor const &color);
	~ColorDialog();
	QColor color() const;
	void setColor(const QColor &color);
private:
	Ui::ColorDialog *ui;
};

#endif // COLORDIALOG_H
