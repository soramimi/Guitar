#include "TextEditDialog.h"
#include "ui_TextEditDialog.h"

#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QSaveFile>
#include <QStringDecoder>

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
	ui->pushButton_cancel->setText(readonly ? tr("&Close") : tr("Cancel"));

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

bool TextEditDialog::readFile(QWidget *parent, QString const &path, QByteArray *data)
{
	data->clear();
	QFile file(path);
	if (!path.isEmpty() && file.open(QFile::ReadOnly)) {
		*data = file.readAll();
		if (file.error() == QFileDevice::NoError) return true;
	} else if (!path.isEmpty() && !QFileInfo::exists(path) && !QFileInfo(path).isSymLink()) {
		return true; // A new file; other open/read failures must never erase old data.
	}
	QMessageBox::warning(parent, tr("Error"), tr("Failed to read '%1':\n%2").arg(path, file.errorString()));
	return false;
}

bool TextEditDialog::writeFile(QWidget *parent, QString const &path, QByteArray const &data)
{
	QSaveFile file(path);
	// Never fall back to truncating the original if creating a temporary fails.
	file.setDirectWriteFallback(false);
	if (file.open(QIODevice::WriteOnly)) {
		if (file.write(data) == data.size()) {
			if (file.commit()) return true;
		} else {
			file.cancelWriting();
		}
	}
	QMessageBox::warning(parent, tr("Error"), tr("Failed to save '%1':\n%2").arg(path, file.errorString()));
	return false;
}

bool TextEditDialog::editFile(QWidget *parent, QString path, QString const &title, QString const &append)
{
	QByteArray data;
	if (!readFile(parent, path, &data)) return false;
	QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
	QString text = decoder(data);
	if (decoder.hasError()) {
		QMessageBox::warning(parent, tr("Error"), tr("Cannot edit '%1' because it is not valid UTF-8.").arg(path));
		return false;
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

	while (dlg.exec() == QDialog::Accepted) {
		if (writeFile(parent, path, dlg.text().toUtf8())) return true;
	}
	return false;
}

