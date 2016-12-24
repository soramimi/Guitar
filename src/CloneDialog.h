#ifndef CLONEDIALOG_H
#define CLONEDIALOG_H

#include <QDialog>
#include "Git.h"

namespace Ui {
class CloneDialog;
}

class CloneDialog : public QDialog
{
	Q_OBJECT
private:
	typedef std::shared_ptr<Git> GitPtr;
	GitPtr git;
	QString default_working_dir;
public:
	explicit CloneDialog(QWidget *parent, GitPtr gitptr, QString const &defworkdir);
	~CloneDialog();

private:
	Ui::CloneDialog *ui;

	// QDialog interface
public slots:
	void accept();
private slots:
	void on_lineEdit_repo_location_textChanged(const QString &arg1);
};

#endif // CLONEDIALOG_H
