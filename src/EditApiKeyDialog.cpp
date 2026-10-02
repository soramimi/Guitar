#include "EditApiKeyDialog.h"
#include "ui_EditApiKeyDialog.h"

EditApiKeyDialog::EditApiKeyDialog(QWidget *parent, QString const &symbol, QString const &apikey)
	: QDialog(parent)
	, ui(new Ui::EditApiKeyDialog)
{
	ui->setupUi(this);
	
	ui->lineEdit_symbol->setText(symbol);
	ui->lineEdit_apikey->setText(apikey);
}

EditApiKeyDialog::~EditApiKeyDialog()
{
	delete ui;
}

std::pair<QString, QString> EditApiKeyDialog::getSymbolAndApiKey() const
{
	QString symbol = ui->lineEdit_symbol->text().trimmed();
	QString apikey = ui->lineEdit_apikey->text().trimmed();
	return {symbol, apikey};
}
