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

	void init(BasicMainWindow *mw, const FileDiffWidget::InitParam_ &param);
	void setTextCodec(QTextCodec *codec);
private:
	Ui::BigDiffWindow *ui;
	void updateDiffView();
};

#endif // BIGDIFFWINDOW_H
