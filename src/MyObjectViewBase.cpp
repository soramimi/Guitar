#include "ApplicationGlobal.h"
#include "GitRunner.h"
#include "MainWindow.h"
#include "MyObjectViewBase.h"
#include "common/joinpath.h"
#include "common/q/helper.h"
#include <QApplication>
#include <QFileDialog>

bool MyObjectViewBase::isValidObject() const
{
	if (misc::starts_with(object_id_, PATH_PREFIX)) return true;
	if (GitHash::isValidID(object_id_)) return true;
	return false; // invalid id
}

bool MyObjectViewBase::saveAs(QWidget *parent)
{
	std::string const &id = object_id_;

	QString path = global->mainwindow->currentWorkingCopyDir() / object_path_;
	QString dstpath = QFileDialog::getSaveFileName(parent, QApplication::tr("Save as"), path);
	if (dstpath.isEmpty()) return false;

	if (misc::starts_with(id, PATH_PREFIX)) {
		QString path = (QS)id.substr(1);
		return global->mainwindow->saveFileAs(path, dstpath);
	} else {
		return global->mainwindow->saveBlobAs(GitHash{id}, dstpath);
	}
}
