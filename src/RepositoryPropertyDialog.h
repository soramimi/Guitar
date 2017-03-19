#ifndef REPOSITORYPROPERTYDIALOG_H
#define REPOSITORYPROPERTYDIALOG_H

#include "RepositoryData.h"
#include "BasicRepositoryDialog.h"
#include <QDialog>
#include "Git.h"


namespace Ui {
class RepositoryPropertyDialog;
}

class RepositoryPropertyDialog : public BasicRepositoryDialog
{
	Q_OBJECT

public:
	explicit RepositoryPropertyDialog(MainWindow *parent, GitPtr g, RepositoryItem const &item);
	~RepositoryPropertyDialog();

private slots:

private:
	Ui::RepositoryPropertyDialog *ui;
};

#endif // REPOSITORYPROPERTYDIALOG_H
