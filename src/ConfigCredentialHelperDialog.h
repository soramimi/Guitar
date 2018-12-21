#ifndef CONFIGCREDENTIALHELPERDIALOG_H
#define CONFIGCREDENTIALHELPERDIALOG_H

#include <QDialog>

namespace Ui {
class ConfigCredentialHelperDialog;
}

class ConfigCredentialHelperDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ConfigCredentialHelperDialog(QWidget *parent = 0);
	~ConfigCredentialHelperDialog();

	QString helper() const;

	void setHelper(QString const &helper);
private:
	Ui::ConfigCredentialHelperDialog *ui;
};

#endif // CONFIGCREDENTIALHELPERDIALOG_H
