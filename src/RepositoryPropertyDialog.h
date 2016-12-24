#ifndef REPOSITORYPROPERTYDIALOG_H
#define REPOSITORYPROPERTYDIALOG_H

#include "RepositoryData.h"
#include <QDialog>

namespace Ui {
class RepositoryPropertyDialog;
}

class RepositoryPropertyDialog : public QDialog
{
	Q_OBJECT

public:
	explicit RepositoryPropertyDialog(QWidget *parent, RepositoryItem const &item);
	~RepositoryPropertyDialog();

private:
	Ui::RepositoryPropertyDialog *ui;
};

#endif // REPOSITORYPROPERTYDIALOG_H
