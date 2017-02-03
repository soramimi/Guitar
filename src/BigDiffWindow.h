#ifndef BIGDIFFWINDOW_H
#define BIGDIFFWINDOW_H

#include <QDialog>
#include "Git.h"
#include "FileDiffWidget.h"

namespace Ui {
class BigDiffWindow;
}

class BigDiffWindow : public QDialog
{
	Q_OBJECT

public:
	explicit BigDiffWindow(QWidget *parent = 0);
	~BigDiffWindow();

	void prepare(MainWindow *mw, FileDiffWidget::ViewStyle view_style, const QByteArray &ba, const Git::Diff &diff, bool uncommited, const QString &workingdir);
private:
	Ui::BigDiffWindow *ui;
};

#endif // BIGDIFFWINDOW_H
