#ifndef INPUTMETHODPOPUP_H
#define INPUTMETHODPOPUP_H

#include "TextEditorWidget.h"

#include <QWidget>

class InputMethodPopup : public QWidget
{
	Q_OBJECT
private:
	struct {
		PreEditText preedit;
	} data;
public:
	explicit InputMethodPopup(QWidget *parent = nullptr);

	void setPreEditText(const PreEditText &preedit);

signals:

public slots:

	// QWidget interface
protected:
	void paintEvent(QPaintEvent *);

	// QWidget interface
public slots:
	void setVisible(bool visible);
};

#endif // INPUTMETHODPOPUP_H
