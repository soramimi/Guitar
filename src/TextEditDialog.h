#ifndef TEXTEDITDIALOG_H
#define TEXTEDITDIALOG_H

#include <QDialog>

namespace Ui {
class TextEditDialog;
}

class TextEditDialog : public QDialog
{
	Q_OBJECT

public:
	explicit TextEditDialog(QWidget *parent = 0);
	~TextEditDialog();

	void setText(QString const &text);
	QString text() const;


	static bool editFile(QWidget *parent, QString path, const QString &title);
private:
	Ui::TextEditDialog *ui;

	// QWidget interface
protected:
	void keyPressEvent(QKeyEvent *);
};

#endif // TEXTEDITDIALOG_H
