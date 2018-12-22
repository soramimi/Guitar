#ifndef SETTINGNETWORKFORM_H
#define SETTINGNETWORKFORM_H

#include "AbstractSettingForm.h"

#include <QWidget>

namespace Ui {
class SettingNetworkForm;
}

class SettingNetworkForm : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingNetworkForm *ui;
	void updateProxyServerLineEdit();
public:
	explicit SettingNetworkForm(QWidget *parent = nullptr);
	~SettingNetworkForm() override;
	void exchange(bool save) override;
private slots:
	void on_radioButton_no_proxy_clicked();
	void on_radioButton_auto_detect_clicked();
	void on_radioButton_manual_clicked();
};

#endif // SETTINGNETWORKFORM_H
