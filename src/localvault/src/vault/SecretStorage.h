#ifndef SECRETSTORAGE_H
#define SECRETSTORAGE_H

#include <string>
#include <vector>

namespace localvault {

/**
 * @brief セキュアストレージバックエンドの抽象インターフェース
 *
 * Vault はこのインターフェースを通じて EMK（Encrypted Master Key）等の
 * 機密データを保存・読み出しする。
 *
 * Qt 非依存。std::vector<char> でバイナリ blob、std::string で識別キーを扱う。
 */
using Blob = std::vector<char>;

/**
 * @brief バックエンド操作の結果
 *
 * 「データが存在しない」と「ストレージにアクセスできない」を区別するために使う。
 * 後者を前者と取り違えると、既存 Vault があるのに新規セットアップへ誘導してしまう。
 */
enum class StorageStatus {
	Ok,
	NotFound, ///< key に対応するデータが存在しない
	Unavailable, ///< ストレージに到達できない・ロックされている等（一時的な可能性あり）
	Error ///< I/O エラー、データ破損等
};

class ISecretStorageBackend {
public:
	virtual ~ISecretStorageBackend() = default;

	/**
	 * @brief このバックエンドが利用可能かを返す
	 */
	virtual bool isAvailable() const = 0;

	/**
	 * @brief バックエンドの識別名
	 */
	virtual std::string name() const = 0;

	/**
	 * @brief key に対応する data を保存する
	 *
	 * 既存データがある場合は置き換える。置き換えはアトミックに行い、
	 * 失敗時に既存データを失ってはならない（削除してから追加する実装は不可）。
	 */
	virtual StorageStatus store(std::string const &key, Blob const &data) = 0;

	/**
	 * @brief key に対応する data を読み出す
	 * @return 存在しない場合は NotFound（out は空）
	 */
	virtual StorageStatus load(std::string const &key, Blob *out) = 0;

	/**
	 * @brief key に対応するデータを削除する
	 * @return 存在しなかった場合は NotFound
	 */
	virtual StorageStatus remove(std::string const &key) = 0;
};

} // namespace localvault

#endif // SECRETSTORAGE_H
