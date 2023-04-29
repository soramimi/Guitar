#include "EditProfileDialog.h"
#include "ui_EditProfileDialog.h"

EditProfileDialog::EditProfileDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::EditProfileDialog)
{
	ui->setupUi(this);
}

EditProfileDialog::~EditProfileDialog()
{
	delete ui;
}

/**
 * @brief プロファイルを追加する
 */
void EditProfileDialog::on_pushButton_add_clicked()
{

}

/**
 * @brief プロファイルを削除する
 */
void EditProfileDialog::on_pushButton_delete_clicked()
{

}

