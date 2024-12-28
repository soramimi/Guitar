#ifndef INPUTNEWTAGDIALOG_H
#define INPUTNEWTAGDIALOG_H

#include <QDialog>

namespace Ui {
class InputNewTagDialog;
}

class InputNewTagDialog : public QDialog {
	Q_OBJECT
private:
	Ui::InputNewTagDialog *ui;
public:
	explicit InputNewTagDialog(QWidget *parent = nullptr);
	~InputNewTagDialog() override;

	QString text() const;
};

#endif // INPUTNEWTAGDIALOG_H
