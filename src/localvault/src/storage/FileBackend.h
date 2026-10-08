#ifndef FILEBACKEND_H
#define FILEBACKEND_H

#include "../vault/SecretStorage.h"
#include <filesystem>
#include <string>

namespace localvault {

/**
 * @brief ファイルベースのセキュアストレージバックエンド
 *
 * OS のセキュアストレージが利用できない環境のフォールバック。
 * EMK をローカルファイルに保存し、可能な限り 0600 等の厳格な
 * パーミッションを設定する。
 *
 * std::filesystem::path と標準 C++ ライブラリで実装。
 */
class FileBackend : public ISecretStorageBackend {
public:
	explicit FileBackend(const std::filesystem::path &directory);

	bool isAvailable() const override;
	std::string name() const override;
	StorageStatus store(std::string const &key, Blob const &data) override;
	StorageStatus load(std::string const &key, Blob *out) override;
	StorageStatus remove(std::string const &key) override;

private:
	std::filesystem::path directory_;

	std::filesystem::path filePathForKey(std::string const &key) const;
	static std::string sanitizeKey(std::string const &key);
	static std::filesystem::path defaultConfigDirectory();
};

} // namespace localvault

#endif // FILEBACKEND_H
