#ifndef SETTINGWindowsFORM_H
#define SETTINGWindowsFORM_H

#include "AbstractSettingForm.h"

namespace Ui {
class SettingWindowsForm;
}

class SettingWindowsForm : public AbstractSettingForm {
	Q_OBJECT
private:
	Ui::SettingWindowsForm *ui;
public:
	explicit SettingWindowsForm(QWidget *parent = nullptr);
	~SettingWindowsForm() override;
	void exchange(bool save) override;
};

#endif // SETTINGWindowsFORM_H
