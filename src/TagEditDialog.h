#ifndef TAGEDITDIALOG_H
#define TAGEDITDIALOG_H

#include <QDialog>

namespace Ui {
class TagEditDialog;
}

class TagEditDialog : public QDialog
{
	Q_OBJECT

public:
	explicit TagEditDialog(QWidget *parent = 0);
	~TagEditDialog();

private:
	Ui::TagEditDialog *ui;
};

#endif // TAGEDITDIALOG_H
