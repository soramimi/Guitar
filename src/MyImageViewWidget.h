#ifndef MYIMAGEVIEWWIDGET_H
#define MYIMAGEVIEWWIDGET_H

#include "ImageViewWidget.h"

// used from FileViewWidget.h

// ImageViewWidgetクラスは、他のアプリで再利用する想定の設計。
// MyImageViewWidgetクラスは、Guitar専用にカスタマイズを行っている。

class MyImageViewWidget : public ImageViewWidget {
private:
	QString object_id_;
	QString path_;
protected:
	void contextMenuEvent(QContextMenuEvent *e) override;
public:
	MyImageViewWidget(QWidget *parent = nullptr);

	void setImage(const QString &mimetype, QByteArray const &ba, QString const &object_id_, QString const &path_);
};

#endif // MYIMAGEVIEWWIDGET_H
