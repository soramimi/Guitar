#include "SelectAiModelPresetDialog.h"
#include "ui_SelectAiModelPresetDialog.h"

using namespace GenerativeAI;


namespace {

enum {
	ModelIndexRole = Qt::UserRole,
};

enum class Column {
	Provider,
	Model,
};

}

struct SelectAiModelPresetDialog::Private {
	std::vector<Model> models;
};

SelectAiModelPresetDialog::SelectAiModelPresetDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SelectAiModelPresetDialog)
	, m(new Private)
{
	ui->setupUi(this);
	
	m->models = ai_model_presets();
	
	QStringList cols = {
		tr("Provider"),
		tr("Model"),
	};
	
	ui->tableWidget->verticalHeader()->hide();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	
	ui->tableWidget->setColumnCount(cols.size());
	for (size_t i = 0; i < cols.size(); i++) {
		ui->tableWidget->setHorizontalHeaderItem(i, new QTableWidgetItem(cols[i]));
	}
	
	ui->tableWidget->setRowCount(m->models.size());
	for (size_t i = 0; i < m->models.size(); i++) {
		ui->tableWidget->setRowHeight(i, 24);
		QString provider = QString::fromStdString(m->models[i].provider_description());
		QString model_name = QString::fromStdString(m->models[i].model_name());
		if (model_name.isEmpty()) {
			model_name = tr("(unknown)");
		}
		QTableWidgetItem *descriontion = new QTableWidgetItem(provider);
		QTableWidgetItem *model = new QTableWidgetItem(model_name);
		model->setData(ModelIndexRole, (int)i);
		ui->tableWidget->setItem(i, (int)Column::Provider, descriontion);
		ui->tableWidget->setItem(i, (int)Column::Model, model);
	}
	
	ui->tableWidget->resizeColumnsToContents();
}

int SelectAiModelPresetDialog::selectedModelIndex() const
{
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < ui->tableWidget->rowCount()) {
		QTableWidgetItem *item = ui->tableWidget->item(row, (int)Column::Model);
		return item->data(ModelIndexRole).toInt();
	}
	return -1;
}

Model SelectAiModelPresetDialog::selectedModel() const
{
	int index = selectedModelIndex();
	if (index >= 0 && index < (int)m->models.size()) {
		return m->models[index];
	}
	return {};
}

SelectAiModelPresetDialog::~SelectAiModelPresetDialog()
{
	delete m;
	delete ui;
}




void SelectAiModelPresetDialog::on_tableWidget_itemDoubleClicked(QTableWidgetItem *item)
{
	done(QDialog::Accepted);
}

