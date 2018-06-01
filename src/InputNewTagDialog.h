#ifndef INPUTNEWTAGDIALOG_H
#define INPUTNEWTAGDIALOG_H

#include <QDialog>

namespace Ui {
class InputNewTagDialog;
}

class InputNewTagDialog : public QDialog
{
	Q_OBJECT

public:
	explicit InputNewTagDialog(QWidget *parent = 0);
	~InputNewTagDialog();

	QString text() const;
private:
	Ui::InputNewTagDialog *ui;
};

#endif // INPUTNEWTAGDIALOG_H
