#ifndef SUBMODULEADDDIALOG_H
#define SUBMODULEADDDIALOG_H

#include <QDialog>
#include <QThread>
#include "Git.h"

namespace Ui {
class SubmoduleAddDialog;
}

class MainWindow;

class SubmoduleAddDialog : public QDialog {
	Q_OBJECT
private:
	Ui::SubmoduleAddDialog *ui;
	struct Private;
	Private *m;

	using GitPtr = std::shared_ptr<Git>;
private:
	MainWindow *mainwindow();
public:
	explicit SubmoduleAddDialog(MainWindow *parent, QString const &url, QString const &defworkdir, const Git::Context *gcx);
	~SubmoduleAddDialog() override;

	QString url();
	QString dir();
	QString overridedSshKey() const;
	bool isForce() const;
private slots:
	void on_pushButton_test_clicked();
	void on_pushButton_browse_clicked();
	void on_pushButton_open_existing_clicked();
	void on_comboBox_remote_currentTextChanged(const QString &text);
};

#endif // SUBMODULEADDDIALOG_H
