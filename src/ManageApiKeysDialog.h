
#include "ApplicationSettings.h"
#ifndef MANAGEAPIKEYSDIALOG_H
#define MANAGEAPIKEYSDIALOG_H

#include <QDialog>

namespace Ui { class ManageApiKeysDialog; }

class ManageApiKeysDialog : public QDialog {
	Q_OBJECT
private:
	Ui::ManageApiKeysDialog *ui;
	
	bool edit(const QString &symbol, const QString &apikey);
public:
	explicit ManageApiKeysDialog(QWidget *parent, AiApiKeys const &keys);
	~ManageApiKeysDialog();
	
	AiApiKeys api_keys() const;
private slots:
	void on_pushButton_add_clicked();
	void on_pushButton_delete_clicked();
	void on_pushButton_edit_clicked();
};

#endif // MANAGEAPIKEYSDIALOG_H
