#ifndef SECUREPINEDIT_H
#define SECUREPINEDIT_H

#include "../vault/SecureBuffer.h"
#include <QWidget>

class QStyleOptionFrame;

// namespace localvault {

/**
 * @brief PIN 専用の入力ウィジェット
 *
 * QLineEdit は入力文字列を内部の QString（通常ヒープ）に保持し、
 * 呼び出し側から確実に消去する手段がない。本ウィジェットは入力を
 * UTF-8 で SecureBuffer に直接格納し、QString には保持しない。
 *
 * 制約（意図的に機能を絞っている）:
 * - カーソルは常に末尾。編集はBackspaceによる末尾削除のみ
 * - クリップボード（コピー・貼り付け）、ドラッグ＆ドロップ、IME は無効
 * - 表示はマスク文字のみ
 *
 * 残留リスク: キーイベントごとに Qt が生成する 1 文字分の QString は消去できない。
 */
class SecurePinEdit : public QWidget {
public:
	explicit SecurePinEdit(QWidget *parent = nullptr);
	~SecurePinEdit() override;

	/** 最大文字数（コードポイント数） */
	void setMaxLength(int length);

	bool isEmpty() const;

	/** 入力内容が other と一致するか */
	bool equals(const SecurePinEdit &other) const;

	/**
	 * @brief 入力内容をメモリロック済みの SecureBuffer として複製する
	 * @return メモリロックに失敗した場合は false。out は空のままにする
	 */
	bool pin(localvault::SecureBuffer *out) const;

	/** 入力内容を消去する */
	void clear();

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void focusInEvent(QFocusEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;

private:
	localvault::SecureBuffer utf8_;
	int length_ = 0;
	int maxLength_ = 64;

	void initStyleOption(QStyleOptionFrame *option) const;
	void appendText(const QString &text);
	void removeLast();
};

// } // namespace localvault

#endif // SECUREPINEDIT_H
