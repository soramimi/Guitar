#ifndef LINEEDITDIALOG_H
#define LINEEDITDIALOG_H

#include <QDialog>

namespace Ui {
class LineEditDialog;
}

class LineEditDialog : public QDialog
{
	Q_OBJECT
public:
	explicit LineEditDialog(QWidget *parent, QString const &title, QString const &prompt, QString const &val, bool password);
	~LineEditDialog();
	QString text() const;
private:
	Ui::LineEditDialog *ui;
};

#endif // LINEEDITDIALOG_H
