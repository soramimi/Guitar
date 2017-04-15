#ifndef ASKPASSDIALOG_H
#define ASKPASSDIALOG_H

#include <QDialog>

namespace Ui {
class AskPassDialog;
}

class AskPassDialog : public QDialog
{
	Q_OBJECT

public:
	explicit AskPassDialog(const QString &caption);
	~AskPassDialog();

	QString text() const;
private:
	Ui::AskPassDialog *ui;
};

#endif // ASKPASSDIALOG_H
