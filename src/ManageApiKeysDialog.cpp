#include "EditApiKeyDialog.h"
#include "ManageApiKeysDialog.h"
#include "ui_ManageApiKeysDialog.h"
#include "common/misc.h"

ManageApiKeysDialog::ManageApiKeysDialog(QWidget *parent, const AiApiKeys &keys)
	: QDialog(parent)
	, ui(new Ui::ManageApiKeysDialog)
{
	ui->setupUi(this);
	
	// api_keys_ = keys;
	
	std::vector<std::pair<std::string, AiApiKeys::Item>> items;
	for (const auto &pair : keys.map) {
		if (pair.first.empty()) continue;
		if (pair.second.api_key.empty()) continue;
		items.push_back(pair);
	}
	std::sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
		auto Compare = [](const auto &a, const auto &b) {
			return misc::stricmp(a.first, b.first);
		};
		return Compare(a, b) < 0;
	});
	
	QStringList columns = { tr("Symbol"), tr("API key") };
	ui->tableWidget->setColumnCount(columns.size());
	
	for (size_t col = 0; col < columns.size(); col++) {
		QTableWidgetItem *item = new QTableWidgetItem(columns[col]);
		ui->tableWidget->setHorizontalHeaderItem(col, item);
	}
	
	ui->tableWidget->setRowCount(items.size());
	
	for (size_t row = 0; row < items.size(); ++row) {
		const auto &pair = items[row];
		const std::string &env_name = pair.first;
		const AiApiKeys::Item &item = pair.second;
		
		QTableWidgetItem *item_symbol = new QTableWidgetItem(QString::fromStdString(env_name));
		ui->tableWidget->setItem(row, 0, item_symbol);
		
		QTableWidgetItem *item_key = new QTableWidgetItem(QString::fromStdString(item.api_key));
		ui->tableWidget->setItem(row, 1, item_key);
	}
	
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	
	ui->tableWidget->verticalHeader()->setVisible(false);	
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
	ui->tableWidget->resizeColumnsToContents();
}

ManageApiKeysDialog::~ManageApiKeysDialog()
{
	delete ui;
}

AiApiKeys ManageApiKeysDialog::api_keys() const
{
	AiApiKeys keys;
	int nrows = ui->tableWidget->rowCount();
	for (int row = 0; row < nrows; ++row) {
		QTableWidgetItem *item_symbol = ui->tableWidget->item(row, 0);
		QTableWidgetItem *item_key = ui->tableWidget->item(row, 1);
		if (item_symbol && item_key) {
			std::string symbol = item_symbol->text().toStdString();
			std::string api_key = item_key->text().toStdString();
			if (!symbol.empty() && !api_key.empty()) {
				AiApiKeys::Item item;
				item.from = AiApiKeys::KeyFrom::Default;
				item.api_key = api_key;
				keys.emplace(symbol, item);
			}
		}
	}
	return keys;
}

bool ManageApiKeysDialog::edit(QString const &symbol, QString const &apikey)
{
	EditApiKeyDialog dlg(this, symbol, apikey);
	if (dlg.exec()) {
		auto [symbol, apikey] = dlg.getSymbolAndApiKey();
		int row = 0;
		while (row < ui->tableWidget->rowCount()) {
			if (ui->tableWidget->item(row, 0)->text() == symbol) { // 既存のシンボルを編集する場合
				ui->tableWidget->item(row, 1)->setText(apikey);
				return true;
			}
			row++;
		}
		// 新しいシンボルを追加する場合
		ui->tableWidget->insertRow(row);
		auto SetText = [&](int row, int col, QString const &text){
			QTableWidgetItem *item = new QTableWidgetItem(text);
			ui->tableWidget->setItem(row, col, item);
		};
		SetText(row, 0, symbol);
		SetText(row, 1, apikey);
		return true;
	}
	return false;
	
}

void ManageApiKeysDialog::on_pushButton_add_clicked()
{
	edit({}, {});
}

void ManageApiKeysDialog::on_pushButton_edit_clicked()
{
	int row = ui->tableWidget->currentRow();
	auto ItemText = [&](int row, int col){
		QTableWidgetItem *item = ui->tableWidget->item(row, col);
		if (item) {
			return item->text();
		}
		return QString();
	};
	
	QString symbol = ItemText(row, 0);
	QString apikey = ItemText(row, 1);
	edit(symbol, apikey);
}

void ManageApiKeysDialog::on_pushButton_delete_clicked()
{
	int row = ui->tableWidget->currentRow();
	if (row >= 0) {
		ui->tableWidget->removeRow(row);
	}
}


