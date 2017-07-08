#include "ExperimentDialog.h"
#include "ui_ExperimentDialog.h"

#include "GitHubAPI.h"
#include "MemoryReader.h"
#include "webclient.h"

#include <QCryptographicHash>

ExperimentDialog::ExperimentDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::ExperimentDialog)
{
	ui->setupUi(this);

	ui->radioButton_github->click();
}

ExperimentDialog::~ExperimentDialog()
{
	delete ui;
}

QImage ExperimentDialog::getIconFromGitHub(QString name)
{
	GitHubAPI github;
	return github.avatarImage(name.toStdString());
}

QImage ExperimentDialog::getIconFromGravatar(QString name)
{
	QCryptographicHash hash(QCryptographicHash::Md5);
	hash.addData(name.trimmed().toLower().toLatin1());
	QByteArray ba = hash.result();
	char tmp[100];
	for (int i = 0; i < ba.size(); i++) {
		sprintf(tmp + i * 2, "%02x", ba.data()[i] & 0xff);
	}
	QString id = tmp;
	QString url = "https://www.gravatar.com/avatar/%1?s=%2";
	url = url.arg(id).arg(256);
	WebContext webcx;
	WebClient web(&webcx);
	if (web.get(WebClient::URL(url.toStdString())) == 200) {
		if (!web.response().content.empty()) {
			MemoryReader reader(web.response().content.data(), web.response().content.size());
			reader.open(MemoryReader::ReadOnly);
			QImage image;
			image.load(&reader, nullptr);
			return image;
		}
	}
	return QImage();
}

void ExperimentDialog::on_pushButton_get_clicked()
{
	QImage image;
	QString text = ui->lineEdit->text();
	if (ui->radioButton_github->isChecked()) {
		image = getIconFromGitHub(text);
	} else if (ui->radioButton_gravatar->isChecked()) {
		image = getIconFromGravatar(text);
	}
	ui->label_image->setPixmap(QPixmap::fromImage(image));
}
