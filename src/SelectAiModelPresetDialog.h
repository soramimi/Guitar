#ifndef SELECTAIMODELPRESETDIALOG_H
#define SELECTAIMODELPRESETDIALOG_H

#include <QDialog>
#include "ai/GenerativeAI.h"



namespace Ui {
class SelectAiModelPresetDialog;
}

class QTableWidgetItem;

class SelectAiModelPresetDialog : public QDialog {
	Q_OBJECT
private:
	Ui::SelectAiModelPresetDialog *ui;
	struct Private;
	Private *m;
public:
	explicit SelectAiModelPresetDialog(QWidget *parent = nullptr);
	~SelectAiModelPresetDialog();
	
	GenerativeAI::Model selectedModel() const;
private slots:
	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);
};

#endif // SELECTAIMODELPRESETDIALOG_H
