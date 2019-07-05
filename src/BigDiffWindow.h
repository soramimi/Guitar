#ifndef BIGDIFFWINDOW_H
#define BIGDIFFWINDOW_H

#include <QDialog>
#include "Git.h"
#include "FileDiffWidget.h"

namespace Ui {
class BigDiffWindow;
}

class BigDiffWindow : public QDialog {
	Q_OBJECT
private:
	struct Private;
	Private *m;
public:
	explicit BigDiffWindow(QWidget *parent = nullptr);
	~BigDiffWindow() override;

	void init(BasicMainWindow *mw, const FileDiffWidget::InitParam_ &param);
	void setTextCodec(QTextCodec *codec);
private:
	Ui::BigDiffWindow *ui;
	void updateDiffView();
	QString fileName() const;
};

#endif // BIGDIFFWINDOW_H
