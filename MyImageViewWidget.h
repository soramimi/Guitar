#ifndef MYIMAGEVIEWWIDGET_H
#define MYIMAGEVIEWWIDGET_H

#include "ImageViewWidget.h"


class MyImageViewWidget : public ImageViewWidget {
private:
	QString object_id_;
	QString path_;
protected:
	void contextMenuEvent(QContextMenuEvent *e);
public:
	MyImageViewWidget(QWidget *parent = 0);

	void setImage(QString mimetype, QByteArray const &ba, QString const &object_id_, QString const &path_);


};

#endif // MYIMAGEVIEWWIDGET_H
