#ifndef SELECTAIMODELDIALOG_H
#define SELECTAIMODELDIALOG_H

#include <QDialog>
#include <QStyledItemDelegate>

#include <ai/GenerativeAI.h>

namespace Ui {
class SelectAiModelDialog;
}

class SelectAiModelDialog : public QDialog {
	Q_OBJECT
	friend class SelectAiModelDelegate;
private:
	Ui::SelectAiModelDialog *ui;
	struct Private;
	Private *m;
	void on_cred_key_method_changed();
	void setLineEditEndpointUrl(const std::string &url);
	void setLineEditApiKey(const std::string &apikey);
	GenerativeAI::Credential credential() const;
	void updateListWidget(const std::string &fav);
	void selectItem(int i);
	void enableSettingsFrame(bool f);
	
	using ModelConf = GenerativeAI::ModelConf;
	
	ModelConf *modelconf(int row);
	ModelConf const *modelconf(int row) const
	{
		return const_cast<SelectAiModelDialog *>(this)->modelconf(row);
	}
	ModelConf *current_modelconf();
	bool set_current_modelconf(const ModelConf &newconf);
	void update_api_endpoint_url();
	static std::string query_api_key(const std::string &symbol, bool env);
	
	QString aimodels_json_path() const;
	
	static void save_api_keys(std::string const &key, std::vector<ModelConf> const &items, const std::map<QString, QString> &api_key_map);
	std::string first_choice_guid() const;
public:
	explicit SelectAiModelDialog(QWidget *parent);
	~SelectAiModelDialog();

	void load();
	void save();
private slots:
	void on_checkBox_show_api_key_clicked();
	void on_comboBox_api_type_currentIndexChanged(int index);
	void on_comboBox_model_currentTextChanged(const QString &arg1);
	void on_comboBox_provider_currentIndexChanged(int index);
	void on_lineEdit_cred_api_key_textChanged(const QString &arg1);
	void on_lineEdit_cred_symbol_textChanged(const QString &arg1);
	void on_lineEdit_cred_symbol_textEdited(const QString &arg1);
	void on_lineEdit_endpoint_url_textChanged(const QString &arg1);
	void on_lineEdit_name_textChanged(const QString &arg1);
	void on_listWidget_items_currentRowChanged(int currentRow);
	void on_pushButton_delete_clicked();
	void on_pushButton_down_clicked();
	void on_pushButton_load_preset_clicked();
	void on_pushButton_new_clicked();
	void on_pushButton_query_models_clicked();
	void on_pushButton_test_hello_clicked();
	void on_pushButton_up_clicked();
	void on_radioButton_cred_custom_clicked();
	void on_radioButton_cred_environ_clicked();
	void on_toolButton_clicked();
	void on_pushButton_manage_api_keys_clicked();
	void on_pushButton_set_as_the_1st_choice_clicked();
	
public slots:
	int exec();
};

#endif // SELECTAIMODELDIALOG_H
