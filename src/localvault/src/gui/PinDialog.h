#ifndef PINDIALOG_H
#define PINDIALOG_H

#include "../vault/SecureBuffer.h"
#include <QDialog>
#include <QString>

class SecurePinEdit;

namespace localvault {

/**
 * @brief PIN 入力ダイアログ
 *
 * Vault の setup / unlock 時に使用する QtWidgets ダイアログ。
 * 入力は SecurePinEdit で受け付け、QString を経由せず SecureBuffer に保持する。
 */
class PinDialog : public QDialog {
	Q_OBJECT

public:
	/**
	 * @brief ダイアログモード
	 */
	enum class Mode {
		Unlock, ///< 既存 Vault の解除
		Setup ///< 初回セットアップ（2回入力で確認）
	};

	explicit PinDialog(Mode mode, QWidget *parent = nullptr);
	~PinDialog() override;

	/**
	 * @brief 入力された PIN を取得する
	 *
	 * 呼び出し側は返された SecureBuffer を適切に管理すること。
	 */
	SecureBuffer pin() const;

	/**
	 * @brief 解除用 PIN をユーザーに求める
	 * @param parent 親ウィジェット
	 * @param ok 入力されたかどうか
	 * @return 入力された PIN。キャンセル時は空
	 */
	static SecureBuffer requestPin(QWidget *parent, bool *ok = nullptr);

	/**
	 * @brief セットアップ用の新規 PIN をユーザーに求める
	 * @param parent 親ウィジェット
	 * @param ok 入力されたかどうか
	 * @return 入力された PIN。キャンセル時は空
	 */
	static SecureBuffer setupPin(QWidget *parent, bool *ok = nullptr);

private:
	Mode mode_;
	SecurePinEdit *pinEdit_ = nullptr;
	SecurePinEdit *confirmEdit_ = nullptr;

	void clearInput();
	void accept() override;
	void reject() override;
};

} // namespace localvault

#endif // PINDIALOG_H
