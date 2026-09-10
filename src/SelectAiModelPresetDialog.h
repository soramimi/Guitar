#ifndef SELECTAIMODELPRESETDIALOG_H
#define SELECTAIMODELPRESETDIALOG_H

#include <QDialog>

namespace Ui {
class SelectAiModelPresetDialog;
}

class SelectAiModelPresetDialog : public QDialog
{
	Q_OBJECT
	
public:
	explicit SelectAiModelPresetDialog(QWidget *parent = nullptr);
	~SelectAiModelPresetDialog();
	
private:
	Ui::SelectAiModelPresetDialog *ui;
};

#endif // SELECTAIMODELPRESETDIALOG_H
