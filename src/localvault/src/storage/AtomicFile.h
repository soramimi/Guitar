#ifndef ATOMICFILE_H
#define ATOMICFILE_H

#include "../vault/SecretStorage.h"
#include <cstddef>
#include <filesystem>

namespace localvault {

/**
 * @brief ファイルバックエンド共通のファイル I/O ヘルパー
 */
namespace AtomicFile {

	/**
	 * @brief 一時ファイルへ書き込み、永続化してから path へ置き換える
	 *
	 * - UNIX: 一時ファイルを O_EXCL / 0600 で作成し、fsync 後に rename、親ディレクトリも fsync
	 * - Windows: CREATE_NEW で作成し、FlushFileBuffers 後に MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)
	 *
	 * 置き換えに失敗した場合、既存の path は変更されない。
	 */
	StorageStatus write(const std::filesystem::path &path, Blob const &data);

	/**
	 * @brief ファイル全体を読み出す
	 * @return 存在しない場合は NotFound。maxSize を超える場合は Error
	 */
	StorageStatus read(const std::filesystem::path &path, Blob *out, size_t maxSize = 1024 * 1024);

	/**
	 * @brief ファイルを削除する
	 * @return 存在しなかった場合は NotFound
	 */
	StorageStatus remove(const std::filesystem::path &path);

} // namespace AtomicFile

} // namespace localvault

#endif // ATOMICFILE_H
