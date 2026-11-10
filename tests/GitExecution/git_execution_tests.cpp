// Retain the application's globals/default settings without running its UI main.
#define main guitar_application_main
#include "../../src/main.cpp"
#undef main
#include <gtest/gtest.h>
#include "../../src/Git.h"
#include <QProcess>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include "../../src/GenerateCommitMessageDialog.h"
#include <QElapsedTimer>
#include "../../src/ai/AiHttp.h"
#include "../../src/GenerateCommitMessageThread.h"
#include <CommandLine.h>
#include <BasicProcessPosix.h>
#include "../../src/ai/CommitMessageGenerator.h"

namespace {
QString executable;
QByteArray bytes(std::vector<char> const &v) { return QByteArray(v.data(), v.size()); }
class GitExecution : public ::testing::Test {
protected:
    QTemporaryDir temp{QDir::tempPath() + "/guitar space\"-XXXXXX"};
    QString root;
    QString gitProgram;
    std::unique_ptr<Git> git;
    QByteArray direct(QStringList args) {
        QProcess p;
        p.start(gitProgram, QStringList{"-C", root} + args);
        EXPECT_TRUE(p.waitForFinished(30000));
        EXPECT_EQ(p.exitCode(), 0) << p.readAllStandardError().constData();
        return p.readAllStandardOutput();
    }
    void write(QString const &name, QByteArray const &data = "changed\n") {
        QFile f(root + '/' + name); ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        ASSERT_EQ(f.write(data), data.size());
    }
    QByteArray read(QString const &name) {
        QFile f(root + '/' + name); EXPECT_TRUE(f.open(QIODevice::ReadOnly)); return f.readAll();
    }
    QStringList staged() {
        QStringList names;
        for (auto const &name : direct({"diff", "--cached", "--name-only", "-z"}).split('\0'))
            if (!name.isEmpty()) names.append(QString::fromUtf8(name));
        return names;
    }
    void SetUp() override {
        ASSERT_TRUE(temp.isValid()); root = temp.path();
        gitProgram = QStandardPaths::findExecutable("git"); ASSERT_FALSE(gitProgram.isEmpty());
        // Test processes never read or modify the user's global/system Git settings.
        qputenv("GIT_CONFIG_GLOBAL", (root + "/global-config").toUtf8());
        qputenv("GIT_CONFIG_NOSYSTEM", "1");
        direct({"init", "-q"}); direct({"config", "user.name", "Test"});
        direct({"config", "user.email", "test@example.invalid"});
        direct({"config", "core.hooksPath", root + "/no-hooks"});
        for (auto name : {"safe", "victim", "safe\" \"victim", "a b.txt", "--all", "a1.txt", "a[1].txt"}) write(name, "base\n");
        direct({"add", "--all"}); direct({"commit", "-qm", "baseline"});
        GitContext ctx; ctx.git_command = gitProgram.toStdString();
        git = std::make_unique<Git>(ctx, root.toStdString(), "", "");
    }
};
}
TEST(CommandArguments, LegacyRoundTripAndNulRejection)
{
    std::vector<std::string> args{"git", "", "a b", "\"", "a\"\"b", "\t\n", "path\\", "日本語"};
    std::string line;
    for (auto const &arg : args) { if (!line.empty()) line += ' '; line += process::command::quote(arg); }
    EXPECT_EQ(process::command::split(line), args);
    EXPECT_THROW(process::command::validate({"git", std::string("a\0b", 3)}), std::invalid_argument);
    EXPECT_THROW(process::command::split("\"unfinished"), std::invalid_argument);
}
TEST(CommandArguments, WindowsQuotesEmptyAndTrailingBackslashes)
{
    EXPECT_EQ(process::command::windowsLine({"git", ""}), "\"git\" \"\"");
    EXPECT_EQ(process::command::windowsLine({"git", "C:\\work tree\\"}), "\"git\" \"C:\\work tree\\\\\"");
    EXPECT_EQ(process::command::windowsLine({"git", "a\\\"b"}), "\"git\" \"a\\\\\\\"b\"");
}
TEST(CommandArguments, NormalAndPtyPreserveArgv)
{
    std::vector<std::string> args{executable.toStdString(), "--echo-argv", "", "a b", "quote\"", "tail\\", "\t\n", "日本語"};
    QByteArray expected;
    for (size_t i = 2; i < args.size(); ++i) expected += QByteArray::fromStdString(args[i]).toHex() + '\n';
    ProcessPosix normal; normal.startArguments(args, false); normal.wait();
    EXPECT_EQ(normal.get_exit_code(), 0); EXPECT_EQ(bytes(normal.stdout_bytes()), expected);
    ProcessPosixPty pty; pty.startArguments(args, "", false); pty.wait();
    EXPECT_EQ(pty.get_exit_code(), 0); EXPECT_EQ(bytes(pty.stdout_bytes()).replace("\r", ""), expected);
    normal.start(process::command::nativeLine(args), false); normal.wait();
    EXPECT_EQ(bytes(normal.stdout_bytes()), expected);
}
TEST_F(GitExecution, ResetQuotedFilenameDoesNotTouchOtherFiles)
{
    write("safe"); write("victim"); write("safe\" \"victim");
    git->resetFile("safe\" \"victim");
    EXPECT_EQ(read("safe"), "changed\n"); EXPECT_EQ(read("victim"), "changed\n");
    EXPECT_EQ(read("safe\" \"victim"), "base\n");
}
TEST_F(GitExecution, StageUnstageLiteralPaths)
{
    write("victim"); write("--all"); write("a[1].txt"); write("a1.txt"); write("safe\" \"victim"); write("a b.txt");
    git->stage("--all"); EXPECT_EQ(staged(), (QStringList{"--all"})); git->unstage("--all");
    ASSERT_TRUE(git->stage(std::vector<std::string>{"a[1].txt", "safe\" \"victim", "a b.txt"}, nullptr));
    EXPECT_EQ(staged(), (QStringList{"a b.txt", "a[1].txt", "safe\" \"victim"}));
    git->unstage(std::vector<std::string>{"a[1].txt", "safe\" \"victim", "a b.txt"}); EXPECT_TRUE(staged().isEmpty());
}
TEST_F(GitExecution, PtyStageUsesSameLiteralArguments)
{
    write("a b.txt"); write("victim"); ProcessPosixPty pty;
    ASSERT_TRUE(git->stage(std::vector<std::string>{"a b.txt"}, &pty)); pty.wait();
    EXPECT_EQ(pty.get_exit_code(), 0); EXPECT_EQ(staged(), (QStringList{"a b.txt"}));
}
TEST_F(GitExecution, EmptyConfigValueIsNotAQuery)
{
    ASSERT_TRUE(git->git("config --local user.name " + Git::quoted_text("")));
    ASSERT_TRUE(git->git("config --local user.email " + Git::quoted_text("")));
    EXPECT_EQ(direct({"config", "--get", "user.name"}), "\n");
    EXPECT_EQ(direct({"config", "--get", "user.email"}), "\n");
}
TEST_F(GitExecution, HistoryAndAiDiffHandleSpaces)
{
    EXPECT_EQ(git->log_file("a b.txt", 10).size(), 1u);
    write("a b.txt"); git->stage("a b.txt");
    auto diff = CommitMessageGenerator::make_diff(gitProgram.toStdString(), root.toStdString(), {});
    EXPECT_NE(diff.find("+changed"), std::string::npos);
    write(" leading\nname.txt", "base\n"); direct({"add", "--", " leading\nname.txt"});
    direct({"commit", "-qm", "special path"}); write(" leading\nname.txt", "special-change\n"); git->stage(" leading\nname.txt");
    diff = CommitMessageGenerator::make_diff(gitProgram.toStdString(), root.toStdString(), {});
    EXPECT_NE(diff.find("+special-change"), std::string::npos);
    EXPECT_THROW(CommitMessageGenerator::make_diff(gitProgram.toStdString(), root.toStdString(), {"invalid-revision", ""}), std::runtime_error);
}
TEST_F(GitExecution, StatusDecodesGitEscapesAndRenameArrows)
{
    for (QString const &name : {QString("quote\"x"), QString("slash\\name"), QString("tab\tname"), QString("line\nname"), QString("a -> b"), QString("日本語")}) {
        write(name);
        auto raw = direct({"status", "--porcelain", "--", name}).trimmed();
        GitFileStatus status(raw.toStdString());
        EXPECT_EQ(QString::fromStdString(status.data.path1), name);
        EXPECT_TRUE(status.data.path2.empty());
    }
    GitFileStatus rename("R  \"old -> \\\"x\" -> \"new -> \\\"x\"");
    EXPECT_EQ(rename.data.path1, "old -> \"x"); EXPECT_EQ(rename.data.path2, "new -> \"x");
}
TEST_F(GitExecution, SshKeyIsShellDataNotCode)
{
    QString ssh = root + "/fake ssh' tool";
    write("fake ssh' tool", "#!/bin/sh\nprintf '%s\\0' \"$@\" >>\"$GUITAR_TEST_SSH_ARGS\"\nexit 1\n"); QFile::setPermissions(ssh, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QString key = root + "/key$(touch shell-marker)'"; write("key$(touch shell-marker)'");
    GitContext ctx; ctx.git_command = gitProgram.toStdString(); ctx.ssh_command = ssh.toStdString();
    Git remote(ctx, root.toStdString(), "", key.toStdString());
    qputenv("GUITAR_TEST_SSH_ARGS", (root + "/ssh-args").toUtf8());
    EXPECT_FALSE(remote.git("ls-remote ssh://example.invalid/repo"));
    qunsetenv("GUITAR_TEST_SSH_ARGS");
    EXPECT_FALSE(QFile::exists(root + "/shell-marker"));
    EXPECT_TRUE(QFile::exists(root + "/ssh-args"));
    EXPECT_TRUE(read("ssh-args").contains(key.toUtf8() + '\0'));
}
TEST_F(GitExecution, AiDiffUsesIndexAndInitialCommit)
{
    write("a b.txt", "STAGED_ONLY\n"); git->stage("a b.txt"); write("a b.txt", "UNSTAGED_PRIVATE\n");
    auto diff = CommitMessageGenerator::make_diff(gitProgram.toStdString(), root.toStdString(), {});
    EXPECT_NE(diff.find("+STAGED_ONLY"), std::string::npos);
    EXPECT_EQ(diff.find("UNSTAGED_PRIVATE"), std::string::npos);
    auto amended = CommitMessageGenerator::make_diff(gitProgram.toStdString(), root.toStdString(), {"", "HEAD"});
    EXPECT_NE(amended.find("+STAGED_ONLY"), std::string::npos);
    EXPECT_NE(amended.find("+base"), std::string::npos);
    QDir fresh(root + "/fresh"); ASSERT_TRUE(QDir().mkpath(fresh.path()));
    QProcess init; init.start(gitProgram, {"-C", fresh.path(), "init", "-q"}); ASSERT_TRUE(init.waitForFinished());
    QFile file(fresh.filePath("new.txt")); ASSERT_TRUE(file.open(QIODevice::WriteOnly)); file.write("FIRST_COMMIT\n"); file.close();
    QProcess add; add.start(gitProgram, {"-C", fresh.path(), "add", "new.txt"}); ASSERT_TRUE(add.waitForFinished());
    diff = CommitMessageGenerator::make_diff(gitProgram.toStdString(), fresh.path().toStdString(), {});
    EXPECT_NE(diff.find("FIRST_COMMIT"), std::string::npos);
}
TEST_F(GitExecution, AiDiffRejectsOversizeAndCancellation)
{
    write("large.txt", QByteArray(CommitMessageGenerator::max_diff_size + 1, 'a'));
    git->stage("large.txt");
    EXPECT_THROW(CommitMessageGenerator::make_diff(gitProgram.toStdString(), root.toStdString(), {}), std::runtime_error);
    auto cancel = std::make_shared<std::atomic_bool>(true);
    EXPECT_THROW(CommitMessageGenerator::make_diff(gitProgram.toStdString(), root.toStdString(), {}, cancel), std::runtime_error);
}
TEST(AiResponse, RequiresCompleteNonemptyMessageSchema)
{
    for (auto text : {"not json", "{}", "{\"messages\":[]}", "{\"unrelated\":[\"wrong\"]}", "{\"messages\":[42]}", "{\"messages\":[\" \"]}", "{\"messages\":[\"partial\"]"}) {
        AiResult ai; ai.d.completed = true; ai.d.content = text;
        EXPECT_TRUE(CommitMessageGenerator::parse_response({}, ai).error) << text;
    }
    AiResult ai; ai.d.completed = true; ai.d.content = R"({"messages":["Good message"]})";
    auto result = CommitMessageGenerator::parse_response({}, ai);
    EXPECT_FALSE(result.error); ASSERT_EQ(result.messages.size(), 1u); EXPECT_EQ(result.messages[0], "Good message");
}
TEST(AiWorker, ImmediateRequestAndExceptionsProduceResult)
{
    GenerateCommitMessageThread worker; QEventLoop loop; bool received = false;
    QObject::connect(&worker, &GenerateCommitMessageThread::ready, &loop, [&](auto const &result) { received = true; EXPECT_FALSE(static_cast<bool>(result)); loop.quit(); }, Qt::QueuedConnection);
    worker.start();
    worker.request({}, {}, [](auto) -> std::string { throw std::runtime_error("test preparation failed"); }, "");
    QTimer::singleShot(2000, &loop, &QEventLoop::quit); loop.exec();
    EXPECT_TRUE(received); worker.stop();
    for (int i = 0; i < 20; ++i) { worker.start(); worker.stop(); }
}
TEST(AiWorker, StopCancelsPreparation)
{
    GenerateCommitMessageThread worker; std::atomic_bool started{false}; worker.start();
    worker.request({}, {}, [&](auto token) { started.store(true); while (!token->load()) std::this_thread::sleep_for(std::chrono::milliseconds(1)); return ""; }, "");
    QElapsedTimer timer; timer.start();
    while (!started.load() && timer.elapsed() < 1000) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(started.load()); timer.restart(); worker.stop(); EXPECT_LT(timer.elapsed(), 500);
}
TEST(AiTransport, CancellationAndDeadlineBoundSilentServer)
{
    QTcpServer server; ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] { auto socket = server.nextPendingConnection(); QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] { socket->readAll(); }); });
    InetClient::Request request("http://127.0.0.1:" + std::to_string(server.serverPort()) + "/");
    auto token = std::make_shared<std::atomic_bool>(false);
    QTimer::singleShot(100, &server, [token] { token->store(true); });
    QElapsedTimer timer; timer.start(); AiHttp http(token);
    EXPECT_EQ(http.get(request), -1); EXPECT_LT(timer.elapsed(), 1000);
    AiHttp deadline({}, 100); timer.restart(); EXPECT_EQ(deadline.get(request), -1); EXPECT_LT(timer.elapsed(), 1000);
}
TEST(AiTransport, LimitsResponseAndDoesNotFollowRedirect)
{
    QTcpServer server; ASSERT_TRUE(server.listen(QHostAddress::LocalHost)); int requests = 0;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, &requests] {
            auto input = socket->readAll(); if (!input.contains("HTTP/")) return; ++requests;
            if (input.startsWith("GET /redirect")) socket->write("HTTP/1.1 302 Found\r\nLocation: /destination\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            else socket->write("HTTP/1.1 200 OK\r\nContent-Length: 4194305\r\nConnection: close\r\n\r\n" + QByteArray(4194305, 'x'));
            socket->disconnectFromHost();
        });
    });
    auto base = "http://127.0.0.1:" + std::to_string(server.serverPort()); AiHttp http;
    EXPECT_EQ(http.get(InetClient::Request(base + "/redirect")), 302); EXPECT_EQ(requests, 1);
    EXPECT_EQ(http.get(InetClient::Request(base + "/large")), -1); EXPECT_EQ(http.content_length(), 0u);
}
TEST(AiDialog, MissingModelAndDeletedDefaultAreDeterministic)
{
    QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
    auto oldSecret = global->app_secret_config_dir; auto oldConfig = global->config_file_path;
    struct Restore { QString secret, config; ~Restore() { global->app_secret_config_dir = secret; global->config_file_path = config; } } restore{oldSecret, oldConfig};
    global->app_secret_config_dir = directory.path(); global->config_file_path = directory.filePath("settings.ini");
    {
        GenerateCommitMessageDialog dialog(nullptr);
        auto combo = dialog.findChild<QComboBox *>("comboBox_ai_models"); ASSERT_NE(combo, nullptr); EXPECT_EQ(combo->currentIndex(), -1);
        auto button = dialog.findChild<QPushButton *>("pushButton_regenerate"); ASSERT_NE(button, nullptr);
        EXPECT_FALSE(dialog.generate()); EXPECT_TRUE(button->isEnabled()); EXPECT_EQ(QApplication::overrideCursor(), nullptr);
    }
    GenerativeAI::ModelConf conf; conf.guid = "present-model"; conf.name = "Test model";
    conf.model = GenerativeAI::Model(GenerativeAI::ProviderID::OpenAI_chat_completions, "test-model");
    GenerativeAI::ModelConf::save(nullptr, global->aimodels_json_path().toStdString().c_str(), {conf});
    { QSettings settings(global->config_file_path, QSettings::IniFormat); settings.setValue("AI/DefaultModelGUID", "deleted-model"); }
    GenerateCommitMessageDialog dialog(nullptr); auto combo = dialog.findChild<QComboBox *>("comboBox_ai_models");
    ASSERT_NE(combo, nullptr); EXPECT_EQ(combo->currentIndex(), 0); EXPECT_EQ(combo->currentText(), "Test model");
}
TEST(AiTransport, SuccessfulLocalCompletionDoesNotLogSecretsOrDiff)
{
    QTcpServer server; ASSERT_TRUE(server.listen(QHostAddress::LocalHost)); QByteArray received;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
            received += socket->readAll();
            if (!received.contains("DUMMY_DIFF_SENTINEL")) return;
            QByteArray body = R"({"object":"chat.completion","choices":[{"message":{"role":"assistant","content":"{\"messages\":[\"Local summary\"]}"},"finish_reason":"stop"}]})";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
        });
    });
    QTemporaryDir logs; QString path = logs.filePath("ai.log"); Logger::start(); Logger::open(path.toStdString());
    GenerativeAI::Model model(GenerativeAI::ProviderID::OpenAI_chat_completions, "test-model");
    model.endpoint_url_ = "http://127.0.0.1:" + std::to_string(server.serverPort()) + "/v1/chat/completions";
    AiApiBridge bridge(model, {"DUMMY_KEY_SENTINEL"});
    auto result = CommitMessageGenerator::parse_response(model, bridge.request("DUMMY_DIFF_SENTINEL", ""));
    Logger::stop();
    EXPECT_FALSE(result.error); ASSERT_EQ(result.messages.size(), 1u); EXPECT_EQ(result.messages[0], "Local summary");
    EXPECT_TRUE(received.contains("DUMMY_KEY_SENTINEL"));
    QFile file(path); ASSERT_TRUE(file.open(QIODevice::ReadOnly)); auto contents = file.readAll();
    EXPECT_FALSE(contents.contains("DUMMY_KEY_SENTINEL")); EXPECT_FALSE(contents.contains("DUMMY_DIFF_SENTINEL"));
}

TEST_F(GitExecution, BatchCommitsPreserveRawParserSemanticsAndOrder)
{
    write("commit-message", "Subject #parent:literal\n\nBody with tabs\tand UTF-8 日本語\n\nAnother paragraph\n");
    direct({"commit", "--allow-empty", "-F", root + "/commit-message"});
    auto ids = git->rev_list_all({}, 100);
    auto result = git->readCommits(ids); ASSERT_TRUE(result); ASSERT_EQ(result->size(), ids.size());
    for (size_t i = 0; i < ids.size(); ++i) {
        auto raw = direct({"cat-file", "-p", QString::fromStdString(ids[i].toString())});
        auto expected = Git::parseCommit(std::vector<char>(raw.begin(), raw.end())); ASSERT_TRUE(expected);
        auto const &actual = (*result)[i];
        EXPECT_EQ(actual.commit_id, ids[i]); EXPECT_EQ(actual.tree, expected->tree);
        EXPECT_EQ(actual.parent_ids, expected->parent_ids); EXPECT_EQ(actual.message, expected->message);
        EXPECT_EQ(actual.author, expected->author); EXPECT_EQ(actual.email, expected->email);
        EXPECT_EQ(actual.commit_date.toSecsSinceEpoch(), expected->commit_date.toSecsSinceEpoch()); EXPECT_EQ(actual.gpgsig, expected->gpgsig);
    }
    std::reverse(ids.begin(), ids.end()); result = git->readCommits(ids); ASSERT_TRUE(result);
    for (size_t i = 0; i < ids.size(); ++i) EXPECT_EQ((*result)[i].commit_id, ids[i]);
}
TEST_F(GitExecution, BatchCommitsPreserveMergeParentsAndSignaturePayload)
{
    auto head = direct({"rev-parse", "HEAD"}).trimmed();
    auto tree = direct({"rev-parse", "HEAD^{tree}"}).trimmed();
    QByteArray raw = "tree " + tree + "\nparent " + head + "\nparent " + head +
        "\nauthor Test <test@example.invalid> 1700000000 +0900\ncommitter Test <test@example.invalid> 1700000001 +0900\n"
        "gpgsig -----BEGIN PGP SIGNATURE-----\n synthetic test payload\n -----END PGP SIGNATURE-----\n"
        "\nMerge subject #literal\n\nMultiline body\n";
    write("raw-commit", raw);
    auto id = direct({"hash-object", "-w", "-t", "commit", root + "/raw-commit"}).trimmed();
    auto result = git->readCommits({GitHash(id.toStdString())}); ASSERT_TRUE(result); ASSERT_EQ(result->size(), 1u);
    auto expected = Git::parseCommit(std::vector<char>(raw.begin(), raw.end())); ASSERT_TRUE(expected);
    EXPECT_EQ((*result)[0].parent_ids, expected->parent_ids); EXPECT_EQ((*result)[0].parent_ids.size(), 2u);
    EXPECT_TRUE((*result)[0].has_gpgsig); EXPECT_EQ((*result)[0].gpgsig, expected->gpgsig);
    EXPECT_EQ((*result)[0].message, expected->message);
}
TEST_F(GitExecution, BatchCommitsDrainLargeOutputAcrossRequestWindows)
{
    write("message", QByteArray(128 * 1024, 'x') + "\n");
    direct({"commit", "--allow-empty", "-F", root + "/message"});
    auto id = git->rev_parse("HEAD"); std::vector<GitHash> ids(150, id);
    auto result = git->readCommits(ids); ASSERT_TRUE(result); ASSERT_EQ(result->size(), ids.size());
    EXPECT_EQ((*result)[0].message.size(), (*result)[149].message.size());
    EXPECT_GE((*result)[149].message.size(), 128 * 1024u);
}
TEST_F(GitExecution, BatchCommitsRejectMissingObjectsAndNoncommits)
{
    auto empty = git->readCommits({}); ASSERT_TRUE(empty); EXPECT_TRUE(empty->empty());
    EXPECT_FALSE(git->readCommits({GitHash("1111111111111111111111111111111111111111")}));
    auto tree = direct({"rev-parse", "HEAD^{tree}"}).trimmed();
    EXPECT_FALSE(git->readCommits({GitHash(tree.toStdString())}));
    EXPECT_FALSE(git->readCommits({GitHash()}));
}
TEST_F(GitExecution, BatchCommitsRejectMalformedFramingAndNonzeroExit)
{
    auto head = git->rev_parse("HEAD"); auto id = QByteArray::fromStdString(head.toString());
    auto raw = direct({"cat-file", "-p", QString::fromLatin1(id)});
    std::vector<QByteArray> responses = {
        id + " commit -1\n", id + " commit 9999999999999999999999\n",
        id + " commit 16777217\n", id + " commit 9\nshort", id + " missing\n",
        QByteArray(200, 'x') + '\n', QByteArray(40, '1') + " commit 0\n\n",
        id + " commit " + QByteArray::number(raw.size()) + '\n' + raw + "!"
    };
    for (auto const &response : responses) {
        write("response", response);
        write("fake-git", "#!/bin/sh\ncat response\n");
        ASSERT_TRUE(QFile::setPermissions(root + "/fake-git", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        GitContext context; context.git_command = (root + "/fake-git").toStdString();
        Git fake(context, root.toStdString(), "", ""); EXPECT_FALSE(fake.readCommits({head}));
    }
    write("response", id + " commit " + QByteArray::number(raw.size()) + '\n' + raw + '\n');
    write("fake-git", "#!/bin/sh\ncat response\nexit 1\n");
    GitContext context; context.git_command = (root + "/fake-git").toStdString();
    Git fake(context, root.toStdString(), "", ""); EXPECT_FALSE(fake.readCommits({head}));
}
TEST_F(GitExecution, HistoryStateTracksAllRefsAndSymbolicHead)
{
    auto original = git->historyState(); ASSERT_TRUE(original); EXPECT_EQ(original, git->historyState());
    auto head = direct({"rev-parse", "HEAD"}).trimmed();
    direct({"tag", "new-tag"}); EXPECT_NE(original, git->historyState());
    direct({"tag", "-d", "new-tag"}); EXPECT_EQ(original, git->historyState());
    direct({"update-ref", "refs/remotes/example/topic", QString::fromLatin1(head)}); EXPECT_NE(original, git->historyState());
    direct({"update-ref", "-d", "refs/remotes/example/topic"}); EXPECT_EQ(original, git->historyState());
    direct({"switch", "-c", "same-commit"}); EXPECT_NE(original, git->historyState());
    auto attached = git->historyState(); direct({"checkout", "--detach"}); EXPECT_NE(attached, git->historyState());
}
TEST_F(GitExecution, HistoryStateDetectsShallowBoundaryChanges)
{
    auto original = git->historyState(); ASSERT_TRUE(original);
    auto head = direct({"rev-parse", "HEAD"}); write(".git/shallow", head);
    auto shallow = git->historyState(); ASSERT_TRUE(shallow); EXPECT_NE(original, shallow);
    ASSERT_TRUE(QFile::remove(root + "/.git/shallow")); EXPECT_EQ(original, git->historyState());
}
TEST_F(GitExecution, LocalFetchNoChangeNewRefAndPruneSnapshots)
{
    auto remote = root + "/remote.git"; direct({"init", "--bare", "-q", remote});
    direct({"remote", "add", "origin", remote}); direct({"push", "origin", "HEAD:refs/heads/main"});
    auto before = git->historyState(); ASSERT_TRUE(before);
    ASSERT_TRUE(git->fetch(nullptr, false)); EXPECT_EQ(before, git->historyState());
    auto head = direct({"rev-parse", "HEAD"}).trimmed();
    direct({"--git-dir=" + remote, "update-ref", "refs/heads/new-topic", QString::fromLatin1(head)});
    ASSERT_TRUE(git->fetch(nullptr, false)); auto added = git->historyState(); EXPECT_NE(before, added);
    direct({"--git-dir=" + remote, "update-ref", "-d", "refs/heads/new-topic"});
    ASSERT_TRUE(git->fetch(nullptr, true)); EXPECT_EQ(before, git->historyState());
}
int main(int argc, char **argv)
{
    if (argc >= 2 && std::string(argv[1]) == "--echo-argv") {
        for (int i = 2; i < argc; ++i) { auto value = QByteArray(argv[i]).toHex(); fwrite(value.data(), 1, value.size(), stdout); putchar('\n'); }
        return 0;
    }
    QApplication app(argc, argv); executable = QCoreApplication::applicationFilePath();
    WebClient::initialize(); ApplicationGlobal state; global = &state;
    if (argc == 3 && std::string(argv[1]) == "--tls-check") {
        WebClient legacy(state.webcx()); InetClient::Request request(argv[2]);
        int oldStatus = legacy.get(request); AiHttp secure; int status = secure.get(request);
        printf("strict=%d legacy=%d ai=%d\n", state.webcx()->is_strict_certificate_verification(), oldStatus, status);
        return !state.webcx()->is_strict_certificate_verification() || oldStatus >= 0 || status >= 0;
    }
    state.file_type_detector = loadPlugin<FileType, FileTypeInterface>("filetypeplugin");
    if (!state.file_type_detector) return 2;
    ::testing::InitGoogleTest(&argc, argv); int result = RUN_ALL_TESTS();
    WebClient::cleanup(); return result;
}
