#ifndef CLEANSUBMODULEDIALOG_H
#define CLEANSUBMODULEDIALOG_H

#include <QDialog>

namespace Ui {
class CleanSubModuleDialog;
}

class CleanSubModuleDialog : public QDialog {
	Q_OBJECT
public:
	struct Options {
		bool reset_hard = false;
		bool clean_df = false;
	};
private:
	Ui::CleanSubModuleDialog *ui;
	Options options_;
public:
	explicit CleanSubModuleDialog(QWidget *parent = nullptr);
	~CleanSubModuleDialog();
	Options options() const;
private slots:
	void on_checkBox_reset_hard_stateChanged(int);
	void on_checkBox_clean_df_stateChanged(int);
};

#endif // CLEANSUBMODULEDIALOG_H
