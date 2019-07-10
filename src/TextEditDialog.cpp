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

void TextEditDialog::setText(QString const &text, bool readonly)
{
	ui->plainTextEdit->setReadOnly(readonly);
	ui->pushButton_ok->setVisible(!readonly);
	ui->pushButton_cancel->setText(readonly ? tr("&Close") : tr("Cacnel"));

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

bool TextEditDialog::editFile(QWidget *parent, QString path, QString const &title, QString const &append)
{
	QString text;

	path.replace('/', QDir::separator());

	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		text += QString::fromUtf8(file.readAll());
	}

	size_t n = text.size();
	if (n > 0 && text[(int)n - 1] != '\n') {
		text += '\n'; // 最後に改行を追加
	}

	text += append;

	TextEditDialog dlg(parent);
	dlg.setWindowTitle(title);
	dlg.ui->lineEdit_path->setVisible(true);
	dlg.ui->lineEdit_path->setText(path);
	dlg.setText(text, false);

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

