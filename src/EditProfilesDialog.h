#ifndef EDITPROFILESDIALOG_H
#define EDITPROFILESDIALOG_H

#include <QDialog>
#include "common/misc.h"
#include "Git.h"

class QTableWidgetItem;

namespace Ui {
class EditProfilesDialog;
}

class EditProfilesDialog : public QDialog {
	Q_OBJECT
public:
	struct Item {
		QString name;
		QString email;
		Item() = default;
		Item(GitUser const &user);
		operator bool () const
		{
			return misc::isValidMailAddress(email);
		}
		bool operator == (Item const &other) const
		{
			return name == other.name && email == other.email;
		}
	};
private:
	Ui::EditProfilesDialog *ui;
	bool enable_double_click_ = true;
	std::vector<Item> list_;
	QString current_email_;
	void resetTableWidget();
	void updateTableWidget(const Item &select);
	void updateUI();
	void updateAvatar(const QString &email, bool request);
public:
	explicit EditProfilesDialog(QWidget *parent = nullptr);
	~EditProfilesDialog();
	void enableDoubleClock(bool f);
	bool saveXML(const QString &path) const;
	bool loadXML(const QString &path);
	Item selectedItem() const;
private slots:
	void avatarReady();
	void on_pushButton_add_clicked();
	void on_pushButton_delete_clicked();
	void on_lineEdit_name_textChanged(const QString &text);
	void on_lineEdit_mail_textChanged(const QString &text);
	void on_pushButton_up_clicked();
	void on_pushButton_down_clicked();
	void on_tableWidget_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
	void on_tableWidget_itemChanged(QTableWidgetItem *item);
	void on_pushButton_get_icon_from_network_clicked();

	void on_tableWidget_itemDoubleClicked(QTableWidgetItem *item);

public slots:
	int exec(Item const &select);
};

#endif // EDITPROFILESDIALOG_H
