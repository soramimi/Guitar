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
private:
	struct Private;
	Private *m;
public:
	explicit BigDiffWindow(QWidget *parent = 0);
	~BigDiffWindow();

	void init(MainWindow *mw, const FileDiffWidget::InitParam_ &param);
private:
	Ui::BigDiffWindow *ui;
};

#endif // BIGDIFFWINDOW_H
