#ifndef SETTINGGENERALFORM_H
#define SETTINGGENERALFORM_H

#include "AbstractSettingForm.h"

#include <QWidget>

namespace Ui {
class SettingGeneralForm;
}

class SettingGeneralForm : public AbstractSettingForm
{
	Q_OBJECT

public:
	explicit SettingGeneralForm(QWidget *parent = 0);
	~SettingGeneralForm();

private:
	Ui::SettingGeneralForm *ui;

	// AbstractSettingForm interface
public:
	void exchange(bool save);
private slots:
	void on_pushButton_browse_default_working_dir_clicked();
};

#endif // SETTINGGENERALFORM_H
