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
	void reflect();

private slots:
	void on_radioButton_no_proxy_clicked();
	void on_radioButton_auto_detect_clicked();
	void on_radioButton_manual_clicked();
	void on_lineEdit_proxy_server_textChanged(const QString &text);
	void on_checkBox_get_committer_icon_toggled(bool checked);
private:
	Ui::SettingNetworkForm *ui;

};

#endif // SETTINGNETWORKFORM_H
