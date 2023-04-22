#ifndef EDITPROFILEDIALOG_H
#define EDITPROFILEDIALOG_H

#include <QDialog>

namespace Ui {
class EditProfileDialog;
}

class EditProfileDialog : public QDialog {
	Q_OBJECT
private:
	Ui::EditProfileDialog *ui;
public:
	explicit EditProfileDialog(QWidget *parent = nullptr);
	~EditProfileDialog();
};

#endif // EDITPROFILEDIALOG_H
