#include "ApplicationGlobal.h"
#include "GitRunner.h"
#include "MainWindow.h"
#include "MyObjectViewBase.h"
#include "common/joinpath.h"
#include <QFileDialog>

bool MyObjectViewBase::isValidObject() const
{
	if (object_id_.startsWith(PATH_PREFIX)) return true;
	if (GitHash::isValidID(object_id_)) return true;
	return false; // invalid id
}

bool MyObjectViewBase::saveAs(QWidget *parent)
{
	QString id = object_id_;

	QString path = global->mainwindow->currentWorkingCopyDir() / object_path_;
	QString dstpath = QFileDialog::getSaveFileName(parent, QApplication::tr("Save as"), path);
	if (dstpath.isEmpty()) return false;

	if (id.startsWith(PATH_PREFIX)) {
		QString path = id.mid(1);
		return global->mainwindow->saveFileAs(path, dstpath);
	} else {
		return global->mainwindow->saveBlobAs(id, dstpath);
	}
}
