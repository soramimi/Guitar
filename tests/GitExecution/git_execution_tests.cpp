// Retain the application's globals/default settings without running its UI main.
#define main guitar_application_main
#include "../../src/main.cpp"
#undef main
#include <gtest/gtest.h>
#include "../../src/Git.h"
#include <QProcess>
#include <QTemporaryDir>
#include <QStandardPaths>
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
    write("a b.txt");
    auto diff = CommitMessageGenerator::make_diff(gitProgram.toStdString(), root.toStdString(), {});
    EXPECT_NE(diff.find("+changed"), std::string::npos);
    write(" leading\nname.txt", "base\n"); direct({"add", "--", " leading\nname.txt"});
    direct({"commit", "-qm", "special path"}); write(" leading\nname.txt", "special-change\n");
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
int main(int argc, char **argv)
{
    if (argc >= 2 && std::string(argv[1]) == "--echo-argv") {
        for (int i = 2; i < argc; ++i) { auto value = QByteArray(argv[i]).toHex(); fwrite(value.data(), 1, value.size(), stdout); putchar('\n'); }
        return 0;
    }
    QApplication app(argc, argv); executable = QCoreApplication::applicationFilePath();
    WebClient::initialize(); ApplicationGlobal state; global = &state;
    state.file_type_detector = loadPlugin<FileType, FileTypeInterface>("filetypeplugin");
    if (!state.file_type_detector) return 2;
    ::testing::InitGoogleTest(&argc, argv); int result = RUN_ALL_TESTS();
    WebClient::cleanup(); return result;
}
