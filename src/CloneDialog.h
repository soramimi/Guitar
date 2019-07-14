#ifndef CLONEDIALOG_H
#define CLONEDIALOG_H

#include <QDialog>
#include <QThread>
#include "Git.h"

namespace Ui {
class CloneDialog;
}

class BasicMainWindow;

class CloneDialog : public QDialog {
	Q_OBJECT
private:
	struct Private;
	Private *m;

	using GitPtr = std::shared_ptr<Git>;
public:
	explicit CloneDialog(BasicMainWindow *parent, QString const &url, QString const &defworkdir);
	~CloneDialog() override;

	enum class Action {
		Clone,
		AddExisting,
	};
	Action action() const;

	QString url();
	QString dir();
private:
	Ui::CloneDialog *ui;

	BasicMainWindow *mainwindow();
private slots:
	void on_lineEdit_repo_location_textChanged(QString const &text);
	void on_pushButton_test_clicked();
	void on_comboBox_currentIndexChanged(int index);
	void on_pushButton_browse_clicked();
	void on_pushButton_open_existing_clicked();
};

#endif // CLONEDIALOG_H
