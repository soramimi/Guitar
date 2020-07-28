#ifndef SUBMODULEUPDATEDIALOG_H
#define SUBMODULEUPDATEDIALOG_H

#include <QDialog>

namespace Ui {
class SubmoduleUpdateDialog;
}

class SubmoduleUpdateDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SubmoduleUpdateDialog(QWidget *parent = nullptr);
	~SubmoduleUpdateDialog();

	bool isInit() const;
	bool isRecursive() const;


private:
	Ui::SubmoduleUpdateDialog *ui;
};

#endif // SUBMODULEUPDATEDIALOG_H
