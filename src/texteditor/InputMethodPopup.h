#ifndef INPUTMETHODPOPUP_H
#define INPUTMETHODPOPUP_H

#include "TextEditorWidget.h"

#include <QWidget>

class InputMethodPopup : public QWidget {
	Q_OBJECT
private:
	struct {
		PreEditText preedit;
	} data;
protected:
	void paintEvent(QPaintEvent *) override;
public:
	explicit InputMethodPopup(QWidget *parent = nullptr);
	void setPreEditText(const PreEditText &preedit);
public slots:
	void setVisible(bool visible) override;
};

#endif // INPUTMETHODPOPUP_H
