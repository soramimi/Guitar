#ifndef PUSHDIALOG_H
#define PUSHDIALOG_H

#include <QDialog>

namespace Ui {
class PushDialog;
}

class PushDialog : public QDialog
{
	Q_OBJECT

public:
	explicit PushDialog(QWidget *parent, QStringList const &remotes, QStringList const &branches);
	~PushDialog();

	enum Action {
		PushSimple,
		PushSetUpstream,
	};

	Action action() const;
	QString remote() const;
	QString branch() const;

private slots:
#if 0
	void on_radioButton_push_simply_clicked();
	void on_radioButton_push_set_upstream_clicked();
#endif
private:
	Ui::PushDialog *ui;
};

#endif // PUSHDIALOG_H
