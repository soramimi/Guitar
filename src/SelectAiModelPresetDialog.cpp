#include "SelectAiModelPresetDialog.h"
#include "ui_SelectAiModelPresetDialog.h"

using namespace GenerativeAI;


namespace {

enum {
	ModelIndexRole = Qt::UserRole,
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
	std::sort(m->models.begin(), m->models.end(), [](Model const &a, Model const &b){
		auto Compare = [](Model const &a, Model const &b){
			if (a.model_name() < b.model_name()) return -1;
			if (a.model_name() > b.model_name()) return 1;
			if (a.provider_description() < b.provider_description()) return -1;
			if (a.provider_description() > b.provider_description()) return 1;
			return 0;
		};
		return Compare(a, b) < 0;
	});
	
	QStringList cols = {
		tr("Model"),
		tr("Provider"),
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
		QTableWidgetItem *model = new QTableWidgetItem(QString::fromStdString(m->models[i].model_name()));
		QTableWidgetItem *descriontion = new QTableWidgetItem(QString::fromStdString(m->models[i].provider_description()));
		model->setData(ModelIndexRole, (int)i);
		ui->tableWidget->setItem(i, 0, model);
		ui->tableWidget->setItem(i, 1, descriontion);
	}
	
	ui->tableWidget->resizeColumnsToContents();
}

Model SelectAiModelPresetDialog::selectedModel() const
{
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < ui->tableWidget->rowCount()) {
		QTableWidgetItem *item = ui->tableWidget->item(row, 0);
		int index = item->data(ModelIndexRole).toInt();
		if (index >= 0 && index < (int)m->models.size()) {
			return m->models[index];
		}
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

