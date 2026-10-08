#include "SecurePinEdit.h"

#include <QKeyEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFrame>

// namespace localvault {

SecurePinEdit::SecurePinEdit(QWidget *parent)
	: QWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setAttribute(Qt::WA_InputMethodEnabled, false);
	setAttribute(Qt::WA_KeyCompression, false);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setCursor(Qt::IBeamCursor);
	
	setMaxLength(64);
}

SecurePinEdit::~SecurePinEdit()
{
	clear();
}

void SecurePinEdit::setMaxLength(int length)
{
	maxLength_ = length;
}

bool SecurePinEdit::isEmpty() const
{
	return utf8_.empty();
}

bool SecurePinEdit::equals(const SecurePinEdit &other) const
{
	return utf8_.equals(other.utf8_);
}

localvault::SecureBuffer SecurePinEdit::pin() const
{
	localvault::SecureBuffer result(utf8_.data(), utf8_.size());
	result.lock();
	return result;
}

void SecurePinEdit::clear()
{
	utf8_.clear();
	length_ = 0;
	update();
}

void SecurePinEdit::appendText(const QString &text)
{
	const int n = text.size();
	for (int i = 0; i < n && length_ < maxLength_; ++i) {
		uint32_t cp = text.at(i).unicode();
		if (QChar::isHighSurrogate(cp) && i + 1 < n && text.at(i + 1).isLowSurrogate()) {
			cp = QChar::surrogateToUcs4(static_cast<char16_t>(cp), text.at(i + 1).unicode());
			++i;
		} else if (QChar::isSurrogate(cp)) {
			continue;
		}
		unsigned char bytes[4];
		size_t len;
		if (cp < 0x80) {
			bytes[0] = static_cast<unsigned char>(cp);
			len = 1;
		} else if (cp < 0x800) {
			bytes[0] = static_cast<unsigned char>(0xc0 | (cp >> 6));
			bytes[1] = static_cast<unsigned char>(0x80 | (cp & 0x3f));
			len = 2;
		} else if (cp < 0x10000) {
			bytes[0] = static_cast<unsigned char>(0xe0 | (cp >> 12));
			bytes[1] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3f));
			bytes[2] = static_cast<unsigned char>(0x80 | (cp & 0x3f));
			len = 3;
		} else {
			bytes[0] = static_cast<unsigned char>(0xf0 | (cp >> 18));
			bytes[1] = static_cast<unsigned char>(0x80 | ((cp >> 12) & 0x3f));
			bytes[2] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3f));
			bytes[3] = static_cast<unsigned char>(0x80 | (cp & 0x3f));
			len = 4;
		}
		// SecureBuffer の拡張時、旧領域はアロケータがゼロクリアする
		utf8_.append(bytes, len);
		localvault::secure_memory::zero(bytes, sizeof(bytes));
		++length_;
	}
}

void SecurePinEdit::removeLast()
{
	size_t size = utf8_.size();
	if (size == 0) return;
	// UTF-8 の継続バイト（10xxxxxx）を飛ばして先頭バイトまで削る
	do {
		--size;
	} while (size > 0 && (utf8_[size] & 0xc0) == 0x80);
	utf8_.resize(size);
	--length_;
}

void SecurePinEdit::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_Backspace:
		removeLast();
		update();
		event->accept();
		return;
	case Qt::Key_Return:
	case Qt::Key_Enter:
	case Qt::Key_Escape:
	case Qt::Key_Tab:
	case Qt::Key_Backtab:
		// ダイアログの既定ボタン・キャンセル・フォーカス移動に任せる
		QWidget::keyPressEvent(event);
		return;
	default:
		break;
	}

	// Ctrl / Cmd ショートカット（貼り付け等）は受け付けない。AltGr（Ctrl+Alt）による文字入力は許可する
	const Qt::KeyboardModifiers mods = event->modifiers();
	const bool shortcut = (mods & (Qt::ControlModifier | Qt::MetaModifier)) && !(mods & Qt::AltModifier);
	const QString text = event->text();
	bool printable = !shortcut && !text.isEmpty();
	for (const QChar c : text) {
		if (!c.isPrint() && !c.isSurrogate()) {
			printable = false;
			break;
		}
	}
	if (printable) {
		appendText(text);
		update();
		event->accept();
		return;
	}
	QWidget::keyPressEvent(event);
}

void SecurePinEdit::initStyleOption(QStyleOptionFrame *option) const
{
	option->initFrom(this);
	option->rect = rect();
	option->lineWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, this);
	option->midLineWidth = 0;
	option->state |= QStyle::State_Sunken;
	option->features = QStyleOptionFrame::None;
}

void SecurePinEdit::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	QStyleOptionFrame option;
	initStyleOption(&option);
	style()->drawPrimitive(QStyle::PE_PanelLineEdit, &option, &painter, this);

	const QRect area = style()->subElementRect(QStyle::SE_LineEditContents, &option, this).adjusted(2, 1, -2, -1);
	const QChar mask(style()->styleHint(QStyle::SH_LineEdit_PasswordCharacter, &option, this));
	const QString masked(length_, mask);
	const int textWidth = fontMetrics().horizontalAdvance(masked);
	// はみ出す場合は末尾が見えるよう右寄せ
	const int x = textWidth > area.width() ? area.right() - textWidth : area.left();

	painter.setClipRect(area);
	painter.setPen(palette().color(QPalette::Text));
	painter.drawText(QRect(x, area.top(), textWidth + 1, area.height()), Qt::AlignLeft | Qt::AlignVCenter, masked);
	if (hasFocus()) {
		const int cursorX = x + textWidth;
		painter.drawLine(cursorX, area.top() + 2, cursorX, area.bottom() - 2);
	}
}

void SecurePinEdit::mousePressEvent(QMouseEvent *event)
{
	setFocus(Qt::MouseFocusReason);
	event->accept();
}

void SecurePinEdit::focusInEvent(QFocusEvent *event)
{
	QWidget::focusInEvent(event);
	update();
}

void SecurePinEdit::focusOutEvent(QFocusEvent *event)
{
	QWidget::focusOutEvent(event);
	update();
}

QSize SecurePinEdit::sizeHint() const
{
	ensurePolished();
	const QFontMetrics fm(font());
	const QSize contents(fm.horizontalAdvance(QLatin1Char('x')) * 17 + 4, qMax(fm.height(), 14) + 2);
	QStyleOptionFrame option;
	initStyleOption(&option);
	return style()->sizeFromContents(QStyle::CT_LineEdit, &option, contents, this);
}

QSize SecurePinEdit::minimumSizeHint() const
{
	const QSize hint = sizeHint();
	return QSize(hint.width() / 2, hint.height());
}

// } // namespace localvault
