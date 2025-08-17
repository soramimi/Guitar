#ifndef SETGLOBALUSERDIALOG_H
#define SETGLOBALUSERDIALOG_H

#include <QDialog>
#include "Git.h"

namespace Ui {
class SetGlobalUserDialog;
}

class SetGlobalUserDialog : public QDialog {
	Q_OBJECT
private:
	Ui::SetGlobalUserDialog *ui;
public:
	explicit SetGlobalUserDialog(QWidget *parent = nullptr);
	~SetGlobalUserDialog() override;

	GitUser user() const;
};

#endif // SETGLOBALUSERDIALOG_H
