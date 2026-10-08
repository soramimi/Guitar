#ifndef CHANGEPINDIALOG_H
#define CHANGEPINDIALOG_H

#include <QDialog>

namespace Ui { class ChangePinDialog; }

class ChangePinDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ChangePinDialog(QWidget *parent = nullptr);
	~ChangePinDialog();

private:
	Ui::ChangePinDialog *ui;
	
	// QDialog interface
public slots:
	
	// QDialog interface
public slots:
	void accept();
};

#endif // CHANGEPINDIALOG_H
