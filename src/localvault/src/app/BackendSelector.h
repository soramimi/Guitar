#ifndef BACKENDSELECTOR_H
#define BACKENDSELECTOR_H

#include "../vault/SecretStorage.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace localvault {

/**
 * @brief EMK の保存先の種類
 */
enum class BackendKind {
	System, ///< OS のセキュアストレージ（SystemKeychainBackend）
	File ///< ファイルフォールバック（FileBackend）
};

/**
 * @brief BackendSelector::select() の結果
 */
struct BackendSelection {
	enum class Status {
		Ok, ///< backend を使用してよい
		NeedsFileConsent, ///< OS ストレージが使えず既存 Vault も見つからない。FileBackend の使用にはユーザーの同意が必要
		RecordedBackendUnavailable, ///< 記録済みの保存先が利用できない。他の保存先へ切り替えてはならない
		ConfigError ///< 保存先の記録が読めない、または内容が不正
	};

	Status status = Status::ConfigError;
	BackendKind kind = BackendKind::System;
	/** Ok / NeedsFileConsent の場合に設定される */
	std::unique_ptr<ISecretStorageBackend> backend;
	/** 保存先が記録済みか。false の場合、セットアップ成功後に record() を呼ぶこと */
	bool recorded = false;
};

/**
 * @brief EMK の保存先を選択・記録する
 *
 * OS のセキュアストレージが一時的に使えない（キーリングのロック、D-Bus 未起動等）だけで
 * FileBackend へ切り替えると、既存 Vault が見えずに別の Vault を作ってしまう。
 * これを防ぐため、使用した保存先を設定ディレクトリに記録し、以後はその保存先のみを使う。
 *
 * 記録がない場合（初回、または旧バージョンからの移行）:
 * - いずれかの保存先に既存 Vault があれば、それを採用して記録する（OS ストレージ優先）
 * - OS ストレージが使えれば、それを候補とする（記録はセットアップ成功後に呼び出し側が行う）
 * - OS ストレージが使えなければ NeedsFileConsent を返す
 */
class BackendSelector {
public:
	using Factory = std::function<std::unique_ptr<ISecretStorageBackend>()>;

	/**
	 * @param configDir 保存先の記録と FileBackend の EMK（configDir/emk）を置くディレクトリ
	 * @param emkKey Vault の EMK キー
	 * @param systemFactory OS ストレージバックエンドの生成関数（省略時は SystemKeychainBackend。テスト用）
	 */
	BackendSelector(const std::filesystem::path &configDir, const std::string &schema, const std::string &emkKey, const Factory &systemFactory = { });
	
	/**
	 * @param force_file_backend true の場合、記録済みの保存先を探索せず FileBackend を選ぶ。
	 *        アプリケーションの永続設定としてのみ使用し、既存 Vault の保存先を途中で変更しないこと。
	 */
	BackendSelection select(bool force_file_backend) const;

	/** 保存先を記録する */
	StorageStatus record(BackendKind kind) const;

	/** 保存先の記録を削除する（Vault 削除後、次回起動時に保存先を選び直すため） */
	StorageStatus forget() const;

	std::unique_ptr<ISecretStorageBackend> create(BackendKind kind) const;

	static const char *name(BackendKind kind);

private:
	std::filesystem::path configDir_;
	std::string schema_;
	std::string emkKey_;
	Factory systemFactory_;
	
	bool hasVault(ISecretStorageBackend *backend) const;
};

} // namespace localvault

#endif // BACKENDSELECTOR_H
