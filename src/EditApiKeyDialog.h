#ifndef EDITAPIKEYDIALOG_H
#define EDITAPIKEYDIALOG_H

#include <QDialog>

namespace Ui { class EditApiKeyDialog; }

class EditApiKeyDialog : public QDialog {
	Q_OBJECT
private:
	Ui::EditApiKeyDialog *ui;
public:
	explicit EditApiKeyDialog(QWidget *parent, QString const &symbol, QString const &apikey);
	~EditApiKeyDialog();
	
	std::pair<QString, QString> getSymbolAndApiKey() const;
};

#endif // EDITAPIKEYDIALOG_H
