#ifndef TEXTEDITDIALOG_H
#define TEXTEDITDIALOG_H

#include <QDialog>

namespace Ui {
class TextEditDialog;
}

class TextEditDialog : public QDialog {
	Q_OBJECT
private:
	Ui::TextEditDialog *ui;
protected:
	void keyPressEvent(QKeyEvent *) override;
public:
	explicit TextEditDialog(QWidget *parent = nullptr);
	~TextEditDialog() override;

	void setText(QString const &text, bool readonly);
	QString text() const;

	static bool editFile(QWidget *parent, QString path, QString const &title, QString const &append = QString());
};

#endif // TEXTEDITDIALOG_H
