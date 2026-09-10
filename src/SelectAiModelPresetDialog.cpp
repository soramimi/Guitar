#include "SelectAiModelPresetDialog.h"
#include "ui_SelectAiModelPresetDialog.h"
#include "ai/GenerativeAI.h"

using namespace GenerativeAI;

SelectAiModelPresetDialog::SelectAiModelPresetDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SelectAiModelPresetDialog)
{
	ui->setupUi(this);

	auto AddModel = [this](Model const &model){
		ui->listWidget->addItem(QString::fromStdString(model.model_uri().string));
	};
	
	std::vector<Model> const &models = ai_model_presets();
	for (Model const &model : models) {
		AddModel(model);
	}
	
}

SelectAiModelPresetDialog::~SelectAiModelPresetDialog()
{
	delete ui;
}
