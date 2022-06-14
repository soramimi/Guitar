#include "AboutDialog.h"
#include "ui_AboutDialog.h"
#include "common/misc.h"

#include <QPainter>
#include <QTextBlock>

#include "version.h"

AboutDialog::AboutDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	misc::setFixedSize(this);

	QString copyright_holder = "S.Fuchita";
	QString twitter_account = "soramimi_jp";

	pixmap.load(":/image/about.png");

	setWindowTitle(tr("About %1").arg(qApp->applicationName()));

	const int FONTSIZE = 14;

	QString html = R"--(
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN" "http://www.w3.org/TR/REC-html40/strict.dtd">
<html>
<head>
<meta name="qrichtext" content="1" />
<meta charset="utf-8" />
<style type="text/css">
p, li { white-space: pre-wrap; }
hr { height: 1px; border-width: 0; }
</style>
</head>
<body style="font-family:'Sans Serif'; font-size:%1px; font-weight:400; font-style:normal;">
<div style="margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:4px; -qt-block-indent:0; text-indent:0px; color: #000000;" >
<p style="text-align: right;">%2</p>
<p style="text-align: right;">Copyright (C) %3 %4</p>
<p style="text-align: right;">%5</p>
<p style="text-align: right;">%6</p>
</div>
</body>
</html>
)--";
	QString devenv = QString("Qt %1").arg(qVersion());
#if defined(_MSC_VER)
	devenv += QString(", msvc=%1").arg(_MSC_VER);
#elif defined(__clang__)
	devenv += QString(", clang=%1.%2").arg(__clang_major__).arg(__clang_minor__);
#elif defined(__GNUC__)
	devenv += QString(", gcc=%1.%2.%3").arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__);
#endif
	html = html
			.arg(FONTSIZE)
			.arg(appVersion())
			.arg(copyright_year)
			.arg(copyright_holder)
			.arg(twitter_account.isEmpty() ? QString() : QString("(@%1)").arg(twitter_account))
			.arg(devenv)
			;
	ui->textBrowser->viewport()->setAutoFillBackground(false);
	ui->textBrowser->setHtml(html);
	QTextDocument *doc =  ui->textBrowser->document();
	QTextCursor textcursor = ui->textBrowser->textCursor();
	for(QTextBlock it = doc->begin(); it != doc->end(); it = it.next()) {
		QTextBlockFormat tbf = it.blockFormat();
		tbf.setLineHeight(FONTSIZE, QTextBlockFormat::FixedHeight);
		textcursor.setPosition(it.position());
		textcursor.setBlockFormat(tbf);
		ui->textBrowser->setTextCursor(textcursor);
	}
	QFont font("Sans Serif");
	font.setPixelSize(FONTSIZE);
	ui->label->setFont(font);
}

AboutDialog::~AboutDialog()
{
	delete ui;
}

void AboutDialog::mouseReleaseEvent(QMouseEvent *)
{
	accept();
}

void AboutDialog::paintEvent(QPaintEvent *)
{
	QPainter pr(this);
	int w = width();
	int h = height();
	pr.drawPixmap(0, 0, w, h, pixmap);
}

QString AboutDialog::appVersion()
{
	return QString("%1, v%2 (%3)").arg(qApp->applicationName()).arg(product_version).arg(source_revision);
}
