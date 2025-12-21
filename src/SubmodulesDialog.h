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
private:
	QString working_dir_;
	std::vector<Submodule> mods_;
public:
	explicit SubmodulesDialog(QWidget *parent, QString workingdir, std::vector<Submodule> const &mods);
	~SubmodulesDialog();

private slots:
	void on_tableWidget_itemSelectionChanged();

	void on_pushButton_open_terminal_clicked();

	void on_pushButton_open_file_manager_clicked();

private:
	Ui::SubmodulesDialog *ui;
	QString absoluteDir(int row) const;
};

#endif // SUBMODULESDIALOG_H
