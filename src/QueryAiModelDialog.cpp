#include "QueryAiModelDialog.h"
#include "ui_QueryAiModelDialog.h"

QueryAiModelDialog::QueryAiModelDialog(QWidget *parent, const AiResult::Models &models, const QString &current_model_name)
	: QDialog(parent)
	, ui(new Ui::QueryAiModelDialog)
{
	ui->setupUi(this);
	
	int index = -1;
	{
		std::string current = current_model_name.toStdString();
		for (size_t i = 0; i < models.list.size(); i++) {
			AiResult::Model const &model = models.list[i];
			if (model.id == current) {
				index = i;
			}
			ui->listWidget->addItem(QString::fromStdString(model.id));
		}
	}
	if (index != -1) {
		ui->listWidget->setCurrentRow(index);
	}
}

QueryAiModelDialog::~QueryAiModelDialog()
{
	delete ui;
}

QString QueryAiModelDialog::selectedModel() const
{
	int row = ui->listWidget->currentRow();
	if (row >= 0 && row < ui->listWidget->count()) {
		QListWidgetItem *item = ui->listWidget->item(row);
		return item->text();
	}
	return {};
}

void QueryAiModelDialog::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
	done(QDialog::Accepted);
}

