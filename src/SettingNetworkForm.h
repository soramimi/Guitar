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
private slots:
	void on_radioButton_no_proxy_clicked();

	void on_radioButton_auto_detect_clicked();

	void on_radioButton_manual_clicked();

private:
	Ui::SettingNetworkForm *ui;
	void updateProxyServerLineEdit();
};

#endif // SETTINGNETWORKFORM_H
