#ifndef REBASEONTODIALOG_H
#define REBASEONTODIALOG_H

#include <QDialog>

namespace Ui {
class RebaseOntoDialog;
}

class RebaseOntoDialog : public QDialog
{
	Q_OBJECT
private:
	Ui::RebaseOntoDialog *ui;
public:
	explicit RebaseOntoDialog(QWidget *parent = nullptr);
	~RebaseOntoDialog();
	int exec(QString const &newbase, QString const &upstream, QString const &branch);
	QString newbase() const;
	QString upstream() const;
	QString branch() const;
};

#endif // REBASEONTODIALOG_H
