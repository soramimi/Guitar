#ifndef QUERYAIMODELDIALOG_H
#define QUERYAIMODELDIALOG_H

#include <QDialog>

#include <ai/AiApiBridge.h>

namespace Ui {
class QueryAiModelDialog;
}

class QListWidgetItem;

class QueryAiModelDialog : public QDialog
{
	Q_OBJECT
	
public:
	explicit QueryAiModelDialog(QWidget *parent, AiResult::Models const &models, QString const &current_model_name);
	~QueryAiModelDialog();

	QString selectedModel() const;
	
private slots:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);
	
private:
	Ui::QueryAiModelDialog *ui;
};

#endif // QUERYAIMODELDIALOG_H
