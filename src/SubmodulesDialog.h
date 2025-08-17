#ifndef SUBMODULESDIALOG_H
#define SUBMODULESDIALOG_H

#include <QDialog>
#include "Git.h"
#include <vector>

namespace Ui {
class SubmodulesDialog;
}

class SubmodulesDialog : public QDialog {
	Q_OBJECT
public:
	struct Submodule {
		GitSubmoduleItem submodule;
		GitCommitItem head;
	};
public:
	explicit SubmodulesDialog(QWidget *parent, std::vector<Submodule> mods);
	~SubmodulesDialog();

private:
	Ui::SubmodulesDialog *ui;
};

#endif // SUBMODULESDIALOG_H
