#ifndef PROCESSHARDENING_H
#define PROCESSHARDENING_H

namespace localvault {

/**
 * @brief プロセスのメモリ内容が外部へ書き出されるのを抑止する
 *
 * - Linux: コアダンプ無効化（RLIMIT_CORE = 0）と PR_SET_DUMPABLE = 0。
 *   後者により同一ユーザーの非特権プロセスからの ptrace アタッチや /proc/<pid>/mem の読み出しも拒否される
 * - macOS: コアダンプ無効化（RLIMIT_CORE = 0）
 * - Windows: 何もしない（WER のダンプ取得はシステム設定に依存する）
 *
 * main() の冒頭、機密情報を扱う前に呼ぶこと。
 *
 * @return すべての設定に成功した場合 true
 */
bool hardenProcess();

} // namespace localvault

#endif // PROCESSHARDENING_H
