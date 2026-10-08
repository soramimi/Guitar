#ifndef SYSTEMKEYCHAINBACKEND_H
#define SYSTEMKEYCHAINBACKEND_H

#include "../vault/SecretStorage.h"

namespace localvault {

/**
 * @brief OS 標準のセキュアストレージを利用するバックエンド
 *
 * プラットフォーム別に以下を使用:
 * - Linux: libsecret (Secret Service API)
 * - macOS: Keychain Services (Security.framework)
 * - Windows: DPAPI + ローカルファイル
 *
 * std::string / std::vector<char> を使用する。
 */
class SystemKeychainBackend : public ISecretStorageBackend {
public:
	SystemKeychainBackend();
	~SystemKeychainBackend() override;

	bool isAvailable() const override;
	std::string name() const override;
	StorageStatus store(std::string const &key, Blob const &data) override;
	StorageStatus load(std::string const &key, Blob *out) override;
	StorageStatus remove(std::string const &key) override;

	/**
	 * @brief Linux (libsecret) で使用するスキーマ名を変更する
	 *
	 * デフォルトは "com.example.localvault.Vault"。
	 */
	void setSchemaName(std::string const &schemaName);

	/**
	 * @brief macOS (Keychain) で使用するサービス名を変更する
	 *
	 * デフォルトは "localvault-vault"。
	 */
	void setServiceName(std::string const &serviceName);

private:
	class Impl;
	Impl *impl_;
};

} // namespace localvault

#endif // SYSTEMKEYCHAINBACKEND_H
