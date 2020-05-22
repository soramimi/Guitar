#ifndef REMOTEADVANCEDOPTIONWIDGET_H
#define REMOTEADVANCEDOPTIONWIDGET_H

#include <QWidget>

namespace Ui {
class RemoteAdvancedOptionWidget;
}

class RemoteAdvancedOptionWidget : public QWidget
{
	Q_OBJECT

public:
	explicit RemoteAdvancedOptionWidget(QWidget *parent = nullptr);
	~RemoteAdvancedOptionWidget();
	void setSshKeyOverrigingEnabled(bool enabled);
	QString sshKey() const;
	void setSshKey(QString const &s);
	void clearSshKey();
private slots:
	void on_pushButton_browse_ssh_key_clicked();
	void on_pushButton_clear_ssh_key_clicked();
private:
	Ui::RemoteAdvancedOptionWidget *ui;
};

#endif // REMOTEADVANCEDOPTIONWIDGET_H
