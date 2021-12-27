#ifndef REMOTEADVANCEDOPTIONWIDGET_H
#define REMOTEADVANCEDOPTIONWIDGET_H

#include <QWidget>

namespace Ui {
class RemoteAdvancedOptionWidget;
}

class RemoteAdvancedOptionWidget : public QWidget
{
	Q_OBJECT
private:
	Ui::RemoteAdvancedOptionWidget *ui;
	void updateInformation();
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
	void on_lineEdit_ssh_key_textChanged(const QString &arg1);
};

#endif // REMOTEADVANCEDOPTIONWIDGET_H
