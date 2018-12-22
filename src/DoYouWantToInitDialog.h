#ifndef DOYOUWANTTOINITDIALOG_H
#define DOYOUWANTTOINITDIALOG_H

#include <QDialog>

namespace Ui {
class DoYouWantToInitDialog;
}

class DoYouWantToInitDialog : public QDialog {
	Q_OBJECT
public:
	explicit DoYouWantToInitDialog(QWidget *parent, QString const &dir);
	~DoYouWantToInitDialog() override;

private slots:
	void on_radioButton_yes_clicked();
	void on_radioButton_no_clicked();
	void on_pushButton_clicked();
private:
	Ui::DoYouWantToInitDialog *ui;
};

#endif // DOYOUWANTTOINITDIALOG_H
