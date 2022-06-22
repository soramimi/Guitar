#include "TextEditorWidget.h"


TextEditorWidget::TextEditorWidget(QWidget *parent)
	: QWidget(parent)
{
	layout_ = new QGridLayout(this);
	layout_->setSpacing(0);
	layout_->setContentsMargins(11, 11, 11, 11);
	layout_->setContentsMargins(0, 0, 0, 0);
	view_ = new TextEditorView(this);
	hsb_ = new QScrollBar(this);
	vsb_ = new QScrollBar(this);
	layout_->addWidget(view_, 0, 0, 1, 1);
	layout_->addWidget(hsb_, 1, 0, 1, 1);
	layout_->addWidget(vsb_, 0, 1, 1, 1);
	hsb_->setOrientation(Qt::Horizontal);
	vsb_->setOrientation(Qt::Vertical);
	view_->bindScrollBar(vsb_, hsb_);

	connect(vsb_, &QScrollBar::valueChanged, [&](){ view_->refrectScrollBar(); });
	connect(hsb_, &QScrollBar::valueChanged, [&](){ view_->refrectScrollBar(); });

	view_->setFocusPolicy(Qt::StrongFocus);
}

