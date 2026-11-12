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
#include "../../src/GitProcessThread.h"
#include "../../src/AvatarLoader.h"
#include "../../src/Logger.h"
#include "../../src/Terminal.h"
#include <future>
#include <QThread>

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
// Isolate hang-prone lifecycle tests: timeout kills and reaps the child;
// no detached thread may continue borrowing stack objects in the parent.
static bool runShutdownChild(char const *filter, int timeout)
{
    if (qEnvironmentVariableIsSet("GUITAR_SHUTDOWN_CHILD")) return false;
    QProcess process;
    auto env = QProcessEnvironment::systemEnvironment(); env.insert("GUITAR_SHUTDOWN_CHILD", "1");
    process.setProcessEnvironment(env);
    process.start(executable, {QString("--gtest_filter=") + filter});
    if (!process.waitForStarted(10000)) { ADD_FAILURE() << "child failed to start"; return true; }
    if (!process.waitForFinished(timeout)) {
        process.kill(); process.waitForFinished(5000);
        ADD_FAILURE() << "shutdown test timed out: " << filter;
    } else {
        EXPECT_EQ(process.exitStatus(), QProcess::NormalExit);
        EXPECT_EQ(process.exitCode(), 0) << process.readAllStandardOutput().constData() << process.readAllStandardError().constData();
    }
    return true;
}
TEST(ThreadShutdown, GitProcessThreadRunsRequestsQueuedWhileBusy)
{
    if (runShutdownChild("ThreadShutdown.GitProcessThreadRunsRequestsQueuedWhileBusy", 10000)) return;
    GitProcessThread worker; worker.start();
    std::mutex mutex; std::condition_variable cv; std::vector<int> ran; bool release = false;
    auto make = [&](int n, bool block) {
        GitCommandRunner r;
        r.d.run = [&, n, block](GitCommandRunner &) {
            std::unique_lock lock(mutex);
            if (block) cv.wait(lock, [&]{ return release; });
            ran.push_back(n); cv.notify_all();
        };
        return r;
    };
    worker.request(make(1, true));
    worker.request(make(2, false)); worker.request(make(3, false));
    auto canceled = worker.request(make(4, false)); worker.cancel(canceled);
    { std::lock_guard lock(mutex); release = true; } cv.notify_all();
    {
        std::unique_lock lock(mutex);
        EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]{ return ran.size() >= 3; }))
            << "requests queued while another request ran were never executed";
        EXPECT_EQ(ran, (std::vector<int>{1, 2, 3}));
    }
    worker.stop();
}
TEST(ThreadShutdown, GitProcessThreadCancelOfRunningRequestKeepsQueue)
{
    if (runShutdownChild("ThreadShutdown.GitProcessThreadCancelOfRunningRequestKeepsQueue", 10000)) return;
    GitProcessThread worker; worker.start();
    std::mutex mutex; std::condition_variable cv; std::vector<int> ran; bool started = false, release = false;
    GitCommandRunner first;
    first.d.run = [&](GitCommandRunner &) {
        std::unique_lock lock(mutex); started = true; cv.notify_all();
        cv.wait(lock, [&]{ return release; }); ran.push_back(1);
    };
    auto id = worker.request(std::move(first));
    { std::unique_lock lock(mutex); ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]{ return started; })); }
    worker.cancel(id);
    GitCommandRunner second;
    second.d.run = [&](GitCommandRunner &) { std::lock_guard lock(mutex); ran.push_back(2); cv.notify_all(); };
    worker.request(std::move(second));
    { std::lock_guard lock(mutex); release = true; } cv.notify_all();
    {
        std::unique_lock lock(mutex);
        EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]{ return ran.size() == 2; }));
        EXPECT_EQ(ran, (std::vector<int>{1, 2}));
    }
    worker.stop();
}
TEST(ThreadShutdown, RepeatedStartStopDoesNotHang)
{
    if (runShutdownChild("ThreadShutdown.RepeatedStartStopDoesNotHang", 65000)) return;
    QTemporaryDir dir; ASSERT_TRUE(dir.isValid());
    std::string log = (dir.path() + "/log.txt").toStdString();
    [&]{
        for (int i = 0; i < 3000; i++) {
            Logger::open(log); Logger::start();
            if (i % 3 == 0) logprint(LOG_DEFAULT, "message");
            Logger::stop();
        }
    }();
    {
        // Paused with queued items, the writer must sleep rather than spin.
        Logger::open(log); Logger::start(); Logger::pause(true);
        logprint(LOG_DEFAULT, "queued before measuring");
        QThread::msleep(50);
        std::clock_t before = std::clock();
        QThread::msleep(500);
        double cpu = double(std::clock() - before) / CLOCKS_PER_SEC;
        EXPECT_LT(cpu, 0.2) << "paused logger consumed CPU while idle";
        Logger::stop();
    }
    [&]{
        Logger::open(log); Logger::start(); Logger::pause(true);
        for (int i = 0; i < 10; i++) logprint(LOG_DEFAULT, "queued while paused");
        Logger::stop();
    }();
    QFile f(QString::fromStdString(log)); ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    EXPECT_TRUE(f.readAll().contains("queued while paused"));
    [&]{
        AvatarLoader loader;
        for (int i = 0; i < 3000; i++) { loader.start(nullptr); loader.stop(); }
    }();
}
TEST(ThreadShutdown, AsyncPtyQueuePreservesOutputAndCompletionOwnership)
{
    if (runShutdownChild("ThreadShutdown.AsyncPtyQueuePreservesOutputAndCompletionOwnership", 10000)) return;
    QObject receiver; ProcessPosixPty pty; GitProcessThread worker; worker.start();
    std::promise<void> started; std::vector<int> callbacks; std::vector<std::string> outputs;
    auto make = [&](int number) {
        GitCommandRunner request; request.d.pty = &pty; request.d.wait_for_completion = true;
        request.d.run = [&, number](GitCommandRunner &request) {
            auto id = request.d.request_id;
            pty.set_completion_callback([&, number, id](bool, std::shared_ptr<void>) {
                QMetaObject::invokeMethod(&receiver, [&, number, id] {
                    pty.wait(); // GUI completion owns reaping before advancing.
                    callbacks.push_back(number); outputs.push_back(pty.get_message());
                    worker.complete(id);
                }, Qt::QueuedConnection);
            }, {});
            pty.clear_message();
            pty.startArguments({"/bin/sh", "-c", number == 1 ? "sleep 0.1; echo first" : "echo second"}, "", true);
            if (number == 1) started.set_value();
        };
        return request;
    };
    worker.request(make(1));
    ASSERT_EQ(started.get_future().wait_for(std::chrono::seconds(2)), std::future_status::ready);
    worker.request(make(2));
    ASSERT_TRUE(worker.wait()); worker.stop();
    EXPECT_EQ(callbacks, (std::vector<int>{1, 2}));
    ASSERT_EQ(outputs.size(), 2u);
    EXPECT_NE(outputs[0].find("first"), std::string::npos); EXPECT_EQ(outputs[0].find("second"), std::string::npos);
    EXPECT_NE(outputs[1].find("second"), std::string::npos); EXPECT_EQ(outputs[1].find("first"), std::string::npos);
}
TEST(ThreadShutdown, CanceledAsyncRequestStillOwnsItsCompletion)
{
    if (runShutdownChild("ThreadShutdown.CanceledAsyncRequestStillOwnsItsCompletion", 10000)) return;
    GitProcessThread worker; worker.start(); std::promise<void> started, second;
    GitCommandRunner first; first.d.wait_for_completion = true;
    first.d.run = [&](GitCommandRunner &) { started.set_value(); };
    auto id = worker.request(std::move(first));
    ASSERT_EQ(started.get_future().wait_for(std::chrono::seconds(2)), std::future_status::ready);
    worker.cancel(id);
    GitCommandRunner next; next.d.run = [&](GitCommandRunner &) { second.set_value(); };
    auto second_id = worker.request(std::move(next)); auto done = second.get_future();
    worker.complete(second_id); // A queued request cannot acknowledge the active one.
    EXPECT_EQ(done.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    worker.complete(id);
    EXPECT_EQ(done.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    worker.stop();
}
TEST(ThreadShutdown, StopReleasesAsyncWaitAndDiscardsPendingRequests)
{
    if (runShutdownChild("ThreadShutdown.StopReleasesAsyncWaitAndDiscardsPendingRequests", 10000)) return;
    GitProcessThread worker; worker.start(); std::promise<void> started; std::atomic<int> ran = 0;
    GitCommandRunner first; first.d.wait_for_completion = true;
    first.d.run = [&](GitCommandRunner &) { started.set_value(); };
    auto id = worker.request(std::move(first));
    ASSERT_EQ(started.get_future().wait_for(std::chrono::seconds(2)), std::future_status::ready);
    GitCommandRunner next; next.d.run = [&](GitCommandRunner &) { ++ran; };
    worker.request(std::move(next)); worker.stop();
    worker.start(); worker.complete(id); EXPECT_TRUE(worker.wait()); worker.stop();
    EXPECT_EQ(ran, 0);
}
TEST(TerminalOpen, DirectoryAndKeyAreNotShellInterpreted)
{
    QTemporaryDir base; ASSERT_TRUE(base.isValid());
    QString dir = base.path() + "/repo $(touch injected-dir) `touch injected-tick` %2";
    ASSERT_TRUE(QDir().mkpath(dir));
    QString out = base.path() + "/out.txt";
    QString key = base.path() + "/key '; touch injected-key; ' $(id)";
    auto saved = global->appsettings;
    global->appsettings.ssh_command = "ssh";
    global->appsettings.terminal_command = QString(
        "/bin/sh -c 'pwd > \"$0\"; eval \"set -- $GIT_SSH_COMMAND\"; printf \"%s\\n\" \"$@\" >> \"$0\"' '%1'").arg(out);
    Terminal::open(dir, key);
    global->appsettings = saved;
    QElapsedTimer t; t.start();
    while (!QFileInfo(out).exists() || QFileInfo(out).size() == 0) {
        ASSERT_LT(t.elapsed(), 10000) << "terminal command did not run";
        QThread::msleep(20);
    }
    QThread::msleep(200);
    QFile f(out); ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    QStringList lines = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    ASSERT_EQ(lines.size(), 4) << lines.join(" | ").toStdString();
    EXPECT_EQ(QFileInfo(lines[0]).canonicalFilePath(), QFileInfo(dir).canonicalFilePath());
    EXPECT_EQ(lines[1], "ssh"); EXPECT_EQ(lines[2], "-i"); EXPECT_EQ(lines[3], key);
    for (QString const &name : {"injected-dir", "injected-tick", "injected-key"}) {
        EXPECT_FALSE(QFileInfo::exists(base.path() + "/" + name)) << name.toStdString();
        EXPECT_FALSE(QFileInfo::exists(dir + "/" + name)) << name.toStdString();
        EXPECT_FALSE(QFileInfo::exists(QDir::currentPath() + "/" + name)) << name.toStdString();
    }
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
