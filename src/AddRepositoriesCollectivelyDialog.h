#ifndef ADDREPOSITORIESCOLLECTIVELYDIALOG_H
#define ADDREPOSITORIESCOLLECTIVELYDIALOG_H

#include <QDialog>

namespace Ui {
class AddRepositoriesCollectivelyDialog;
}

class AddRepositoriesCollectivelyDialog : public QDialog {
	Q_OBJECT
private:
	Ui::AddRepositoriesCollectivelyDialog *ui;

public:
	explicit AddRepositoriesCollectivelyDialog(QWidget *parent, const QStringList &dirs);
	~AddRepositoriesCollectivelyDialog();

	QStringList selectedDirs() const;
};

#endif // ADDREPOSITORIESCOLLECTIVELYDIALOG_H
