#include "EditProfilesDialog.h"
#include "ui_EditProfilesDialog.h"
#include "ApplicationGlobal.h"
#include "BlockSignals.h"
#include "RepositoryInfo.cpp"
#include "UserEvent.h"
#include <QFile>
#include <QXmlStreamWriter>

EditProfilesDialog::Item::Item(const GitUser &user)
	: name(user.name)
	, email(user.email)
{
}


EditProfilesDialog::EditProfilesDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::EditProfilesDialog)
{
	ui->setupUi(this);
	global->avatar_loader.connectAvatarReady(this, &EditProfilesDialog::avatarReady);
}

EditProfilesDialog::~EditProfilesDialog()
{
	global->avatar_loader.disconnectAvatarReady(this, &EditProfilesDialog::avatarReady);
	delete ui;
}

void EditProfilesDialog::enableDoubleClock(bool f)
{
	enable_double_click_ = f;
}

int EditProfilesDialog::exec(const Item &select)
{
	resetTableWidget();
	updateTableWidget(select);
	if (list_.size() > 0) {
		ui->tableWidget->selectRow(0);
		ui->lineEdit_name->setEnabled(true);
		ui->lineEdit_mail->setEnabled(true);
	} else {
		ui->lineEdit_name->setEnabled(false);
		ui->lineEdit_mail->setEnabled(false);
	}
	return QDialog::exec();
}

void EditProfilesDialog::updateAvatar(QString const &email, bool request)
{
	current_email_ = email;
	if (current_email_.isEmpty()) {
		ui->widget_avatar_icon->setImage({});
	} else {
		auto icon = global->avatar_loader.fetch(current_email_, request);
		if (!icon.isNull()) {
			ui->widget_avatar_icon->setImage(icon);
		}
	}
}

void EditProfilesDialog::avatarReady()
{
	updateAvatar(current_email_, false);
}

/**
 * @brief テーブルウィジェットを初期化
 */
void EditProfilesDialog::resetTableWidget()
{
	BlockSignals s(ui->tableWidget);
	ui->tableWidget->clear();
	ui->tableWidget->setColumnCount(2);
	ui->tableWidget->verticalHeader()->setVisible(false);
	ui->tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem(tr("Name")));
	ui->tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem(tr("Mail Address")));
	ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}

/**
 * @brief UI更新
 */
void EditProfilesDialog::updateUI()
{
	int row = ui->tableWidget->currentRow();
	ui->pushButton_delete->setEnabled(row >= 0);
	ui->pushButton_up->setEnabled(row > 0);
	ui->pushButton_down->setEnabled(row >= 0 && row + 1 < (int)list_.size());
	bool f = (row >= 0 && row < (int)list_.size());
	ui->lineEdit_name->setText(f ? list_[row].name : QString());
	ui->lineEdit_mail->setText(f ? list_[row].email : QString());
	ui->lineEdit_name->setEnabled(f);
	ui->lineEdit_mail->setEnabled(f);
}

/**
 * @brief list_ の内容をテーブルウィジェットへ反映する
 */
void EditProfilesDialog::updateTableWidget(const Item &select)
{
	resetTableWidget();
	int sel = -1;
	{
		BlockSignals b(ui->tableWidget);
		ui->tableWidget->setRowCount((int)list_.size());
		for (int row = 0; row < (int)list_.size(); row++) {
			Item const &item = list_[row];
			if (item == select) {
				sel = row;
			}
			ui->tableWidget->setItem(row, 0, new QTableWidgetItem(item.name));
			ui->tableWidget->setItem(row, 1, new QTableWidgetItem(item.email));
		}
	}
	updateUI();
	if (sel >= 0) {
		ui->tableWidget->selectRow(sel);
	}
}

/**
 * @brief プロファイルを追加する
 */
void EditProfilesDialog::on_pushButton_add_clicked()
{
	int row = (int)list_.size();
	list_.push_back({});

	{
		BlockSignals b(ui->tableWidget);
		ui->tableWidget->insertRow(row);
		ui->tableWidget->setItem(row, 0, new QTableWidgetItem());
		ui->tableWidget->setItem(row, 1, new QTableWidgetItem());
		ui->tableWidget->selectRow(row);
	}

	{
		BlockSignals b(ui->lineEdit_name);
		ui->lineEdit_name->clear();
		ui->lineEdit_name->setEnabled(true);
	}

	{
		BlockSignals b(ui->lineEdit_mail);
		ui->lineEdit_mail->clear();
		ui->lineEdit_mail->setEnabled(true);
	}

	ui->lineEdit_name->setFocus();
}

/**
 * @brief プロファイルを削除する
 */
void EditProfilesDialog::on_pushButton_delete_clicked()
{
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < (int)list_.size()) {
		list_.erase(list_.begin() + row);
		updateTableWidget({});
		ui->tableWidget->selectRow(row);
	}
	row = ui->tableWidget->currentRow();
	if (row < 0) {
		updateAvatar({}, false);
	}
	updateUI();
}

/**
 * @brief 名前が編集されたとき
 * @param arg1
 */
void EditProfilesDialog::on_lineEdit_name_textChanged(const QString &text)
{
	int row = ui->tableWidget->currentRow();
	if (row < 0) return;

	auto *item = ui->tableWidget->item(row, 0);
	Q_ASSERT(item);
	Q_ASSERT(row < list_.size());
	QString t = text.trimmed();
	list_[row].name = t;
	bool b = ui->tableWidget->blockSignals(true);
	item->setText(t);
	ui->tableWidget->blockSignals(b);
}

/**
 * @brief メールアドレスが編集されたとき
 * @param arg1
 */
void EditProfilesDialog::on_lineEdit_mail_textChanged(const QString &text)
{
	int row = ui->tableWidget->currentRow();
	if (row < 0) return;

	auto *item = ui->tableWidget->item(row, 1);
	Q_ASSERT(item);
	Q_ASSERT(row < list_.size());
	QString mail = text.trimmed();
	list_[row].email = mail;
	{
		BlockSignals b(ui->tableWidget);
		item->setText(mail);
	}
}

void EditProfilesDialog::on_pushButton_up_clicked()
{
	int row = ui->tableWidget->currentRow();
	if (row > 0) {
		std::swap(list_[row - 1], list_[row]);
	}
	updateTableWidget({});
	ui->tableWidget->selectRow(row - 1);
}


void EditProfilesDialog::on_pushButton_down_clicked()
{
	int row = ui->tableWidget->currentRow();
	if (row + 1 < ui->tableWidget->rowCount()) {
		std::swap(list_[row], list_[row + 1]);
	}
	updateTableWidget({});
	ui->tableWidget->selectRow(row + 1);
}

void EditProfilesDialog::on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous)
{
	(void)current;
	(void)previous;

	updateUI();

	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < (int)list_.size()) {
		current_email_ = list_[row].email;
		updateAvatar(current_email_, true);
	}
}

void EditProfilesDialog::on_tableWidget_itemChanged(QTableWidgetItem *item)
{
	int row = ui->tableWidget->currentRow();
	int col = ui->tableWidget->currentColumn();
	Q_ASSERT(row >= 0 && row < (int)list_.size());
	Q_ASSERT(ui->tableWidget->item(row, col) == item);
	QString text = item->text();
	switch (col) {
	case 0:
		list_[row].name = text;
		ui->lineEdit_name->setText(text);
		break;
	case 1:
		list_[row].email = text;
		ui->lineEdit_mail->setText(text);
		break;
	}
}

bool EditProfilesDialog::loadXML(QString const &path)
{
	std::vector<Item> list;
	QFile file(path);
	if (file.open(QFile::ReadOnly)) {
		Item item;
		QString text;
		XmlTagState state;
		QXmlStreamReader reader(&file);
		reader.setNamespaceProcessing(false);
		while (!reader.atEnd()) {
			QXmlStreamReader::TokenType tt = reader.readNext();
			switch (tt) {
			case QXmlStreamReader::StartElement:
				state.push(reader.name());
				if (state == "/profiles/profile") {
					item = {};
				}
				text.clear();
				break;
			case QXmlStreamReader::EndElement:
				if (state == "/profiles/profile") {
					list.push_back(item);
				} else if (state == "/profiles/profile/name") {
					item.name = text.trimmed();
				} else if (state == "/profiles/profile/email") {
					item.email = text.trimmed();
				}
				state.pop();
				break;
			case QXmlStreamReader::Characters:
				text.append(reader.text());
				break;
			}
		}
		list_ = list;
		return true;
	}
	return false;
}

EditProfilesDialog::Item EditProfilesDialog::selectedItem() const
{
	int row = ui->tableWidget->currentRow();
	if (row >= 0 && row < (int)list_.size()) {
		return list_[row];
	}
	return {};
}

bool EditProfilesDialog::saveXML(QString const &path) const
{
	QFile file(path);
	if (file.open(QFile::WriteOnly)) {
		QXmlStreamWriter writer(&file);
		writer.setAutoFormatting(true);
		writer.writeStartDocument();
		writer.writeStartElement("profiles");
		for (Item const &item : list_) {
			writer.writeStartElement("profile");
			{
				writer.writeStartElement("name");
				writer.writeCharacters(item.name.trimmed());
				writer.writeEndElement();
			}
			{
				writer.writeStartElement("email");
				writer.writeCharacters(item.email.trimmed());
				writer.writeEndElement();
			}
			writer.writeEndElement(); // profile
		}
		writer.writeEndElement(); // profiles
		writer.writeEndDocument();
		return true;
	}
	return false;
}

void EditProfilesDialog::on_pushButton_get_icon_from_network_clicked()
{
	updateAvatar(ui->lineEdit_mail->text(), true);
}


void EditProfilesDialog::on_tableWidget_itemDoubleClicked(QTableWidgetItem *item)
{
	(void)item;
	if (enable_double_click_) {
		done(QDialog::Accepted);
	}
}

