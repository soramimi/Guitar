#include "Logger.h"
#include <common/fmt.h>
#include <common/joinpath.h>
#include <common/jstream.h>
#include <common/misc.h>
#include <common/str.h>
#include <ai/CommitMessageGenerator.h>
#include "MyProcess.h"
#include <exception>
#include <stdexcept>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QElapsedTimer>

#ifdef APP_GUITAR
#include "Profile.h"
#include "GitRunner.h"
#include "ApplicationGlobal.h"
#endif



/**
 * @brief AIレスポンスのJSON文字列を解析してコミットメッセージ候補を取り出す。
 * @param in AIから返ってきたレスポンス文字列（JSON）
 * @param provider 使用したAIプロバイダー
 * @return コミットメッセージ候補のリスト、またはエラー情報
 */
CommitMessageGenerator::CommitMessageGenerator::Result CommitMessageGenerator::parse_response(GenerativeAI::Model model, AiResult const &result)
{
	if (result) {
		Result ret;
		QJsonParseError error;
		auto json = QJsonDocument::fromJson(QByteArray::fromStdString(result.content()), &error);
		if (error.error == QJsonParseError::NoError && json.isObject() && json.object().value("messages").isArray()) {
			auto values = json.object().value("messages").toArray();
			bool valid = !values.isEmpty() && values.size() <= 5;
			for (auto const &value : values) {
				QString message = value.toString().trimmed();
				if (!value.isString() || message.isEmpty() || message.size() > 4096 || message.contains(QChar(0))) valid = false;
				ret.messages.push_back(message.toStdString());
			}
			if (valid) return ret;
		}
		ret.messages.clear(); ret.error = true;
		ret.error_status = "Invalid AI response";
		ret.error_message = "Expected a complete JSON object with 1 to 5 nonempty message strings.";
		return ret;
	} else {
		// AIがエラーを返した場合
		CommitMessageGenerator::Result ret;
		ret.error = true;
		ret.error_status = result.error_status();
		ret.error_message = result.error_message();
		if (ret.error_message.empty()) {
			ret.error_message = result.content(); // エラー詳細が取れない場合はレスポンス全体を返す
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
std::pair<std::string, std::string> CommitMessageGenerator::generatePrompt() const
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
)---";

	// optional hint
	if (!request_.hint.empty()) {
		prompt += "# additional hint\n";
		prompt += "- " + request_.hint + "\n\n";
	}

	if (!request_.status_s_u.empty()) {
		prompt += "# git status -s -u\n";
		prompt += request_.status_s_u + "\n\n";
	}

	// build final prompt
	prompt += schema;
	prompt += '\n';
	prompt += "# git diff\n";
	prompt += request_.diff;
	
	std::string prefill = R"-("{"messages":[")-";
	
	return { prompt, prefill };
}

AiResult CommitMessageGenerator::request()
{
	if (request_.diff.size() > max_diff_size || request_.status_s_u.size() > max_diff_size || request_.hint.size() > 4096)
		return AiResult::Error("Input too large", "The commit diff or hint exceeds the size limit.");
	auto [prompt, prefill] = generatePrompt();
	
	prefill = {}; // 対応していないモデルがあるのでprefillは使わない
	
	return api_.request(prompt, prefill);
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
static std::string git(std::string const &gitcommand, std::string const &dir, std::string const &cmd,
                       std::shared_ptr<std::atomic_bool> const &cancelled, int *exit_code = nullptr)
{
	QProcess proc;
	QStringList args{"--literal-pathspecs", "-C", QString::fromStdString(dir.empty() ? "." : dir)};
	for (auto const &arg : process::command::split(cmd)) args.append(QString::fromStdString(arg));
	proc.start(QString::fromStdString(gitcommand), args);
	proc.closeWriteChannel();
	QElapsedTimer timer; timer.start();
	std::string out, error;
	while (true) {
		proc.waitForFinished(20);
		if ((cancelled && cancelled->load()) || timer.elapsed() > 30000) {
			proc.kill(); proc.waitForFinished(); throw std::runtime_error("Git diff cancelled or timed out.");
		}
		auto chunk = proc.readAllStandardOutput();
		if (chunk.size() > CommitMessageGenerator::max_diff_size - out.size()) {
			proc.kill(); proc.waitForFinished(); throw std::runtime_error("Commit diff exceeds the 200000 byte limit.");
		}
		out.append(chunk.data(), chunk.size());
		auto err = proc.readAllStandardError();
		if (error.size() < 4096) error.append(err.data(), std::min<size_t>(err.size(), 4096 - error.size()));
		if (proc.state() == QProcess::NotRunning) break;
	}
	int code = proc.exitStatus() == QProcess::NormalExit && proc.error() != QProcess::FailedToStart ? proc.exitCode() : -1;
	if (exit_code) *exit_code = code;
	else if (code != 0) throw std::runtime_error("Git diff failed: " + error);
	return out;
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
std::string CommitMessageGenerator::make_diff(std::string const &gitcommand, std::string const &dir,
                                               CommitPair const &commits, std::shared_ptr<std::atomic_bool> cancelled)
{
	// HEAD/index for normal commits; parent/index for amend. An absent base
	// means an initial/root commit, for which --cached compares to the empty tree.
	std::string base = commits.a;
	if (base == "HEAD") {
		int code = 0;
		git(gitcommand, dir, "rev-parse --verify HEAD", cancelled, &code);
		if (code != 0) base.clear();
	}
	if (base.empty()) {
		base = QString::fromStdString(git(gitcommand, dir, "hash-object -t tree --stdin", cancelled)).trimmed().toStdString();
	}
	std::string cmd = "diff --cached --no-ext-diff --no-textconv --full-index";
	if (!base.empty()) cmd += " " + process::command::quote(base);
	cmd += " --";
	// Git emits text patches and binary summaries, never binary patch payloads.
	return git(gitcommand, dir, cmd, cancelled);
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
std::string CommitMessageGenerator::make_diff(GitRunner g, const CommitPair &commits, std::shared_ptr<std::atomic_bool> cancelled)
{
	std::string gitcommand = global->appsettings.git_command.toStdString();
	std::string dir = g.workingDir();
	std::string diff = CommitMessageGenerator::make_diff(gitcommand, dir, commits, std::move(cancelled));
	return diff;
}
#endif

