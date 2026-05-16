#include "CommitMessageGenerator.h"
#include "Logger.h"
#include "common/fmt.h"
#include "common/str.h"
#include "common/joinpath.h"
#include "common/jstream.h"
#include "common/q/helper.h"
#include "curlclient.h"
#include "webclient.h"
#include "AiApiBridge.h"

#ifdef APP_GUITAR
#include "Profile.h"
#include "GitRunner.h"
#include "ApplicationGlobal.h"
static inline std::string global_mimetype_by_file(std::string const &path)
{
	return global->mimetype_by_file(path);

}
#else
std::string global_mimetype_by_file(std::string const &path);
#endif



/**
 * @brief AIレスポンスのJSON文字列を解析してコミットメッセージ候補を取り出す。
 * @param in AIから返ってきたレスポンス文字列（JSON）
 * @param provider 使用したAIプロバイダー
 * @return コミットメッセージ候補のリスト、またはエラー情報
 */
CommitMessageGenerator::CommitMessageGenerator::Result CommitMessageGenerator::parse_response(GenerativeAI::Model model, AiResult const &result)
{
	if (result.completion) {
		if (0) {
			// 旧実装：箇条書き形式（"- message" や "1. message"）をパースしていた。
			// 現在はJSON形式に移行したため使用しない。
			std::vector<std::string_view> lines = misc::splitLinesV(result.content);
			size_t i = lines.size();
			while (i > 0) {
				i--;
				std::string_view sv = lines[i];
				char const *ptr = sv.data();
				char const *end = ptr + sv.size();
				// 行頭・行末のバッククォートを除去
				while (ptr + 1 < end && *ptr == '`' && end[-1] == '`') {
					ptr++;
					end--;
				}
				bool accept = false;

				if (ptr < end && *ptr == '-') {
					// "- message" 形式
					accept = true;
					ptr++;
					while (ptr < end && (*ptr == '-' || isspace((unsigned char)*ptr))) { // e.g. "- - message"
						ptr++;
					}
				} else if (isdigit((unsigned char)*ptr)) {
					// "1. message" 形式
					while (ptr < end && isdigit((unsigned char)*ptr)) {
						accept = true;
						ptr++;
					}
					if (ptr < end && *ptr == '.') {
						ptr++;
					}
				}
				if (accept) {
					// 先頭の空白を除去
					while (ptr < end && isspace((unsigned char)*ptr)) {
						ptr++;
					}
					// 前後のダブルクォートを除去
					if (ptr + 1 < end && *ptr == '\"' && end[-1] == '\"') {
						ptr++;
						end--;
					}
					// 前後のアスタリスク（**bold**）を除去
					while (ptr + 1 < end && *ptr == '*' && end[-1] == '*') {
						ptr++;
						end--;
					}
					if (ptr < end) {
						// ok
					} else {
						accept = false;
					}
				}
				if (accept) {
					lines[i] = std::string_view(ptr, end - ptr);
				} else {
					lines.erase(lines.begin() + i);
				}
			}
			std::vector<std::string> ret;
			for (auto const &line : lines) {
				ret.emplace_back(line);
			}
			return ret;
		} else {
			auto TrimPrefix = [](std::string_view const &sv, std::string_view prefix) {
				std::string_view ret = sv;
				if (sv.size() >= prefix.size() && sv.substr(0, prefix.size()) == prefix) {
					ret.remove_prefix(prefix.size());
				}
				return ret;
			};
			// 現行実装：{"messages": ["msg1", "msg2", ...]} 形式のJSONを解析する
			std::vector<std::string> messages;
			std::string_view text = result.content;
			// text = TrimPrefix(text, "```json");
			// text = TrimPrefix(text, "```");
			// JSONオブジェクトの開始位置を前から探してテキストを切り詰める（前に余分なテキストが混入しても対応できるように）
			auto i = text.find_first_of("{[");
			if (i != std::string_view::npos) {
				text.remove_prefix(i);
			}
			// JSONオブジェクトの範囲を前後から絞り込む（前後に余分なテキストが混入しても対応できるように）
			auto j = text.rfind('}');
			if (j != std::string::npos) {
				text = text.substr(0, j + 1);
			}
			jstream::Reader reader(text);
			while (reader.next()) {
				if (reader.match("{*[") || reader.match("[")) { // 期待するパターンは "{messages[" だが、指示を守らない奴がいるので、柔軟に受ける。
					if (reader.isstring()) {
						messages.push_back(reader.string());
					}
				}
			}
			return messages;
		}
	} else {
		// AIがエラーを返した場合
		CommitMessageGenerator::Result ret;
		ret.error = true;
		ret.error_status = result.error_status;
		ret.error_message = result.error_message;
		if (ret.error_message.empty()) {
			ret.error_message = result.content; // エラー詳細が取れない場合はレスポンス全体を返す
		}
		return ret;
	}
}

/**
 * @brief diffからAIへ送るプロンプト文字列を生成する。
 *
 * AIにJSONフォーマット（{"messages": [...]}）で返すよう指示する。
 * @param diff コミット対象のdiff文字列
 * @param max 生成するコミットメッセージ候補の最大数
 * @return AIに送るプロンプト文字列
 */
std::string CommitMessageGenerator::generatePrompt() const
{
	// main prompt
	std::string prompt = fmt(R"---(You are an experienced engineer.
Generate exactly %d concise git commit message candidates written in present tense for the following code diff.
Return ONLY valid and strict JSON. No explanations, no extra text.
Do NOT wrap the output in code fences (e.g., ``` or ```json).

)---")(request_.max_message_count);

	// JSON schema instruction
	std::string schema = R"---(# Schema:
{
  "messages": [
	"message1",
	"message2",
	"message3",
	...
  ]
}

# git diff
)---";

	// optional hint
	if (!request_.hint.empty()) {
		prompt += "Additional hint: " + request_.hint + "\n\n";
	}

	// build final prompt
	prompt += schema;
	prompt += request_.diff;

	return prompt;
}


/**
 * @brief ファイルのdiffをAIに送るべきか判定する。
 *
 * 画像やバイナリ、PDFなどは行単位のdiffが意味をなさないため除外する。
 * Qtの翻訳ファイル（*.ts）も行番号変化が多く差分がノイズになるため除外する。
 * @param filename ファイル名
 * @param mimetype ファイルのMIMEタイプ
 * @return diff対象に含めるべきならtrue、そうでなければfalse
 */
bool CommitMessageGenerator::accept_file_diff(std::string const &filename, std::string const &mimetype)
{
	if (misc::starts_with(mimetype, "image/")) return false; // 画像ファイルはdiffしない
	if (mimetype == "application/octetstream") return false; // バイナリファイルはdiffしない
	if (mimetype == "application/pdf") return false; // PDFはdiffしない
	if (mimetype == "text/xml" && misc::ends_with(filename, ".ts")) return false; // Do not diff Qt translation TS files (line numbers and other changes are too numerous)
	return true;
}

/**
 * @brief HEADとの差分を取得する内部実装。
 *
 * 各ファイルのdiffをスレッドで並列取得する。ただし libfile 内部の realloc が
 * クラッシュすることがあったため、現在はシングルスレッドで動作させている。
 * @param g GitRunner インスタンス
 * @param fn_accept ファイルをdiff対象に含めるか判定するコールバック
 * @return 連結されたdiff文字列
 */
static std::string git(std::string const &gitcommand, std::string const &dir, std::string const &cmd)
{
	Process proc;
	char const *cd = ".";
	if (!dir.empty()) {
		cd = dir.c_str();
	}
	proc.start(fmt("\"%s\" -C \"%s\" %s")(gitcommand)(cd)(cmd), false);
	proc.wait();
	std::string s = (misc::str)proc.stdout_bytes();
	return s;
}

/**
 * @brief コミット差分を取得する。
 *
 * コマンドラインの git を呼び出して diff を取得する内部実装。GitRunner を引数に取らないオーバーロード。
 * @param gitcommand gitコマンドのパス（例: "/usr/bin/git" または "C:\\Program Files\\Git\\bin\\git.exe"）
 * @param dir gitコマンドを実行するディレクトリ（例: リポジトリのルートディレクトリ）
 * @param id_a 比較対象のコミットID（例: "HEAD"）
 * @param id_b 比較対象のコミットID（例: "" なら作業ツリーとの差分）
 * @return 連結されたdiff文字列
 */
std::string CommitMessageGenerator::make_diff(std::string const &gitcommand, std::string const &dir, CommitPair const &commits)
{
#ifdef APP_GUITAR
	PROFILE;
#endif

	Q_ASSERT(!commits.a.empty());
	bool amend = !commits.b.empty();

	// std::string id_a = "HEAD";
	// std::string id_b = {};

	std::string diff;
	std::vector<std::string> names;
	{
		std::string s;
		if (amend) {
			s = git(gitcommand, dir, fmt("diff --name-only %s %s")(commits.a)(commits.b));
		} else {
			s = git(gitcommand, dir, fmt("diff --name-only %s")(commits.a));
		}
		names = (misc::strlist)misc::splitLinesV(s);
	}
	std::vector<std::string> diffs(names.size());
	const int NUM_THREADS = 1;
	std::vector<std::thread> threads(NUM_THREADS);
	std::atomic_size_t index(0);
	for (size_t t = 0; t < threads.size(); t++) {
		threads[t] = std::thread([&](){
			while (1) {
				size_t i = index++;
				if (i >= names.size()) break;
				std::string path = names[i];
				if (path.empty()) continue;
				std::string filename = misc::filename(path);
				std::string fullpath = dir / path;
				std::string mimetype = global_mimetype_by_file(fullpath);
				if (filename == "libtool") continue; // libtoolはdiffしても大きい上に役に立たない
				if (!CommitMessageGenerator::accept_file_diff(path, mimetype)) continue;
				// std::string diff = g.diff_full_index_head_file(fullpath);
				std::string diff;
				if (amend) {
					diff = git(gitcommand, dir, fmt("diff --full-index %s %s")(commits.a)(commits.b));
				} else {
					diff = git(gitcommand, dir, fmt("diff --full-index %s -- %s")(commits.a)(fullpath));
				}
				logprintf(LOG_DEFAULT, "diff %s (mimetype: %s) size: %d\n", path.c_str(), mimetype.c_str(), (int)diff.size());
				if (diff.empty()) continue;
				diffs[i] = diff;
			}
		});
	}

	for (size_t t = 0; t < threads.size(); t++) {
		threads[t].join();
	}

	// スレッドごとの結果を元のファイル順に連結する
	for (size_t i = 0; i < names.size(); i++) {
		if (diffs[i].empty()) continue;
		diff += diffs[i];
	}

	logprintf(LOG_DEFAULT, "total diffs size: %d\n", (int)diff.size());

	return diff;
}

/**
 * @brief コミット差分を取得する。
 *
 * GitRunner を引数に取るオーバーロード。内部で git コマンドを呼び出している。
 * @param g GitRunner インスタンス
 * @param id_a 比較対象のコミットID（例: "HEAD"）
 * @param id_b 比較対象のコミットID（例: "" なら作業ツリーとの差分）
 * @return 連結されたdiff文字列
 */
#ifdef APP_GUITAR
std::string CommitMessageGenerator::make_diff(GitRunner g, const CommitPair &commits)
{
	std::string gitcommand = global->appsettings.git_command.toStdString();
	std::string dir = g.workingDir();
	std::string diff = CommitMessageGenerator::make_diff(gitcommand, dir, commits);
	return diff;
}
#endif
