#ifndef SETTINGNETWORKFORM_H
#define SETTINGNETWORKFORM_H

#include "AbstractSettingForm.h"

#include <QWidget>

namespace Ui {
class SettingNetworkForm;
}

class SettingNetworkForm : public AbstractSettingForm
{
	Q_OBJECT
public:
	explicit SettingNetworkForm(QWidget *parent = 0);
	~SettingNetworkForm();
	void exchange(bool save);
private:
	Ui::SettingNetworkForm *ui;
};

#endif // SETTINGNETWORKFORM_H
