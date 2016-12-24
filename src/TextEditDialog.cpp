#include "TextEditDialog.h"
#include "ui_TextEditDialog.h"

#include <QDir>

TextEditDialog::TextEditDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::TextEditDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui->lineEdit_path->setVisible(false);

	ui->plainTextEdit->setFocus();
}

TextEditDialog::~TextEditDialog()
{
	delete ui;
}

void TextEditDialog::setText(const QString &text)
{
	ui->plainTextEdit->setPlainText(text);
	QTextCursor cur = ui->plainTextEdit->textCursor();
	cur.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
	ui->plainTextEdit->setTextCursor(cur);
}

QString TextEditDialog::text() const
{
	return ui->plainTextEdit->toPlainText();
}


void TextEditDialog::keyPressEvent(QKeyEvent *event)
{
	if (event->modifiers() & Qt::ControlModifier) {
		if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
			event->accept();
			accept();
			return;
		}
	}
	QDialog::keyPressEvent(event);
}

bool TextEditDialog::editFile(QWidget *parent, QString path, QString const &title)
{
	QString text;

	path.replace('/', QDir::separator());

	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		text += QString::fromUtf8(file.readAll());
	}

	TextEditDialog dlg(parent);
	dlg.setWindowTitle(title);
	dlg.ui->lineEdit_path->setVisible(true);
	dlg.ui->lineEdit_path->setText(path);
	dlg.setText(text);

	bool ok = false;

	if (dlg.exec() == QDialog::Accepted) {
		text = dlg.text();
		QFile file(path);
		if (file.open(QFile::WriteOnly)) {
			file.write(text.toUtf8());
			ok = true;
		}
	}

	return ok;
}
