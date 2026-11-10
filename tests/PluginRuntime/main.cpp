#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QLibrary>
#include <QTemporaryDir>
#include <QFile>
#include <QDebug>
#include "fixtures/Service.h"
#include "../../src/LoadPlugin.h"
#include <cstring>
#include <memory>
#include <algorithm>
#include "../../subprojects/FileTypePlugin/src/zs.h"
#include "../../subprojects/FileTypePlugin/src/FileTypeInterface.h"
#include "../../subprojects/IncrementalSearchPlugin/src/IncrementalSearchInterface.h"
#include "../../subprojects/IncrementalSearchPlugin/src/romaji.h"
#include "../../subprojects/IncrementalSearchPlugin/mecab/mecab/src/mecab.h"

bool x_get_resource_i8(char const *, std::vector<char> *);
bool x_get_resource_i16(char const *, std::vector<short int> *);

namespace {
std::string encode(std::string const &plain)
{
    ZS zs;
    size_t pos = 0;
    std::string encoded;
    bool ok = zs.compress([&](char *p, int capacity) {
        size_t n = std::min(static_cast<size_t>(capacity), plain.size() - pos);
        memcpy(p, plain.data() + pos, n); pos += n; return static_cast<int>(n);
    }, [&](char const *p, int n) { encoded.append(p, n); return n; });
    EXPECT_TRUE(ok) << zs.error;
    return encoded;
}
bool decode(std::string const &encoded, std::string &plain, size_t limit = -1, size_t chunk = 1)
{
    ZS zs;
    size_t pos = 0;
    return zs.decompress([&](char *p, int capacity) {
        size_t n = std::min({static_cast<size_t>(capacity), chunk, encoded.size() - pos});
        memcpy(p, encoded.data() + pos, n); pos += n; return static_cast<int>(n);
    }, [&](char const *p, int n) { plain.append(p, n); return n; }, limit);
}
QString pluginPath(char const *name)
{
    return QCoreApplication::applicationDirPath() + "/../lib/soramimi.jp/lib" + name + ".so";
}
}

TEST(PluginLoader, ReportsMissingFactoryAndInitializationErrors)
{
    QString error;
    auto missing = loadPlugin<TestService, TestPluginInterface>("missing-test-plugin", &error);
    EXPECT_FALSE(missing);
    EXPECT_TRUE(error.contains("missing-test-plugin"));
    struct Failure { char const *mode; char const *message; };
    for (auto failure : {Failure{"null", "factory returned null"}, Failure{"create-throw", "factory failed"},
                         Failure{"open-fail", "initialization failed"}, Failure{"open-throw", "dictionary decode failed"}}) {
        auto mode = failure.mode;
        qputenv("GUITAR_TEST_PLUGIN_MODE", mode);
        auto service = loadPlugin<TestService, TestPluginInterface>("loaderfixture", &error);
        EXPECT_FALSE(service) << mode;
        EXPECT_TRUE(error.contains("loaderfixture"));
        EXPECT_TRUE(error.contains(failure.message)) << error.toStdString();
    }
    qunsetenv("GUITAR_TEST_PLUGIN_MODE");
    EXPECT_TRUE((loadPlugin<TestService, TestPluginInterface>("loaderfixture", &error)));
    EXPECT_TRUE(error.isEmpty());
}

TEST(ZstdStream, RoundTripAndEmptyFrame)
{
    for (std::string const &text : {std::string(), std::string(400000, 'x'), std::string("日本語")}) {
        std::string out;
        EXPECT_TRUE(decode(encode(text), out));
        EXPECT_EQ(out, text);
    }
}
TEST(ZstdStream, ConcatenatedFrames)
{
    std::string out;
    EXPECT_TRUE(decode(encode("first") + encode("second"), out));
    EXPECT_EQ(out, "firstsecond");
}
TEST(ZstdStream, RejectsTruncatedCorruptAndEmptyInput)
{
    auto encoded = encode(std::string(1000, 'x'));
    encoded.pop_back();
    std::string out;
    EXPECT_FALSE(decode(encoded, out));
    out.clear(); EXPECT_FALSE(decode("not zstd", out));
    out.clear(); EXPECT_FALSE(decode("", out));
}
TEST(ZstdStream, EnforcesLimitBeforeWriting)
{
    std::string out;
    auto encoded = encode(std::string(400000, 'x'));
    EXPECT_FALSE(decode(encoded, out, 10, encoded.size()));
    EXPECT_LE(out.size(), 10u);
    out.clear(); EXPECT_TRUE(decode(encode("123"), out, 3)); EXPECT_EQ(out, "123");
    out.clear(); EXPECT_FALSE(decode(encode("x"), out, 0)); EXPECT_TRUE(out.empty());
}
TEST(ZstdStream, RejectsReadAndWriteFailures)
{
    ZS zs;
    auto failRead = [](char *, int) { return -1; };
    auto write = [](char const *, int n) { return n; };
    EXPECT_FALSE(zs.decompress(failRead, write));
    EXPECT_FALSE(zs.compress(failRead, write));
    auto encoded = encode("hello");
    size_t pos = 0;
    auto read = [&](char *p, int capacity) {
        size_t n = std::min(static_cast<size_t>(capacity), encoded.size() - pos);
        memcpy(p, encoded.data() + pos, n); pos += n; return static_cast<int>(n);
    };
    EXPECT_FALSE(zs.decompress(read, [](char const *, int n) { return n - 1; }));
    pos = 0;
    EXPECT_FALSE(zs.compress(read, [](char const *, int n) { return n - 1; }));
}
TEST(Romaji, RespectsViewBoundary)
{
    char incomplete[] = {'k'};
    EXPECT_EQ(convert_romaji_to_katakana(std::string_view(incomplete, 1)), "");
    char a[] = {'a'};
    EXPECT_EQ(convert_romaji_to_katakana(std::string_view(a, 1)), "ア");
    EXPECT_EQ(convert_romaji_to_katakana(std::string_view()), "");
    EXPECT_EQ(convert_romaji_to_katakana(std::string_view("nekoka", 4)), "ネコ");
    EXPECT_EQ(convert_romaji_to_katakana("wagahaihanekodearu"), "ワガハイハネコデアル");
    EXPECT_EQ(convert_romaji_to_katakana("konnichiha"), "コンニチハ");
}
TEST(FileTypePlugin, DetectsLargeSparseFiles)
{
    QPluginLoader loader(pluginPath("filetypeplugin"));
    auto iface = qobject_cast<FileTypeInterface *>(loader.instance());
    ASSERT_NE(iface, nullptr) << loader.errorString().toStdString();
    std::unique_ptr<FileType> detector(iface->create());
    ASSERT_TRUE(detector->open());
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile file(dir.filePath("large"));
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    for (qint64 size : {qint64(0), qint64(1), qint64(2147483647), qint64(2147483648), qint64(4294967296)}) {
        ASSERT_TRUE(file.resize(size));
        auto path = file.fileName().toStdString();
        EXPECT_NO_THROW({ EXPECT_FALSE(detector->detect(path.c_str()).mimetype.isEmpty()); });
    }
}
TEST(FileTypePlugin, RejectsCorruptDatabaseAndCanRetry)
{
    QPluginLoader loader(pluginPath("filetypeplugin"));
    auto iface = qobject_cast<FileTypeInterface *>(loader.instance());
    ASSERT_NE(iface, nullptr);
    QLibrary library(pluginPath("filetypeplugin"));
    auto data = reinterpret_cast<unsigned char *>(library.resolve("magic_mgc_zst"));
    ASSERT_NE(data, nullptr);
    struct Restore { unsigned char *p; unsigned char value; ~Restore() { *p = value; } } restore{data, *data};
    *data = 0;
    std::unique_ptr<FileType> detector(iface->create());
    EXPECT_FALSE(detector->open());
    EXPECT_TRUE(detector->detect("Hello, world", 12).mimetype.isEmpty());
    *data = restore.value;
    ASSERT_TRUE(detector->open());
    EXPECT_EQ(detector->detect("Hello, world", 12).mimetype, "text/plain");
    EXPECT_TRUE(detector->open());
}
TEST(IncrementalSearchPlugin, RejectsCorruptDictionary)
{
    QPluginLoader loader(pluginPath("incrementalsearchplugin"));
    auto iface = qobject_cast<IncrementalSearchInterface *>(loader.instance());
    ASSERT_NE(iface, nullptr);
    QLibrary library(pluginPath("incrementalsearchplugin"));
    auto data = reinterpret_cast<unsigned char *>(library.resolve("xxd_sysdic_zst"));
    ASSERT_NE(data, nullptr);
    struct Restore { unsigned char *p; unsigned char value; ~Restore() { *p = value; } } restore{data, *data};
    *data = 0;
    std::vector<char> previous{'o', 'k'};
    EXPECT_FALSE(x_get_resource_i8("sys.dic", &previous));
    EXPECT_EQ(previous, (std::vector<char>{'o', 'k'}));
    std::unique_ptr<IncrementalSearch> search(iface->create());
    // Repeat failure and recovery on the same engine; LSan checks ownership
    // of dictionaries whose open() failed before Tokenizer registered them.
    for (int i = 0; i < 8; ++i) {
        *data = 0;
        EXPECT_FALSE(search->open());
        *data = restore.value;
        ASSERT_TRUE(search->open());
        EXPECT_TRUE(static_cast<bool>(search->match("吾輩は猫である", search->makeFilter("neko"))));
        search.reset(iface->create());
    }
}
TEST(IncrementalSearchPlugin, RejectsUnregisteredUserDictionariesWithoutLeaking)
{
    for (int i = 0; i < 8; ++i) {
        // Missing resource fails open(); a system dictionary fails the user
        // type check even though it opened successfully. Both precede dic_.
        for (auto args : {"-d /dummy/ -u /dummy/missing.dic", "-d /dummy/ -u /dummy/sys.dic"}) {
            std::unique_ptr<MeCab::Tagger> tagger(MeCab::createTagger(args));
            EXPECT_EQ(tagger, nullptr) << args;
        }
        std::unique_ptr<MeCab::Tagger> valid(MeCab::createTagger("-d /dummy/"));
        ASSERT_NE(valid, nullptr);
        ASSERT_NE(valid->parseToNode("吾輩は猫である"), nullptr);
    }
}
TEST(IncrementalSearchPlugin, RejectsOddSizedMatrix)
{
    QLibrary library(pluginPath("incrementalsearchplugin"));
    auto data = reinterpret_cast<unsigned char *>(library.resolve("xxd_matrixbin_zst"));
    auto length = reinterpret_cast<unsigned int *>(library.resolve("xxd_matrixbin_zst_len"));
    ASSERT_NE(data, nullptr); ASSERT_NE(length, nullptr);
    auto encoded = encode("x");
    ASSERT_LT(encoded.size(), *length);
    struct Restore {
        unsigned char *data; unsigned int *length; unsigned int size; std::string bytes;
        ~Restore() { memcpy(data, bytes.data(), bytes.size()); *length = size; }
    } restore{data, length, *length, std::string(reinterpret_cast<char *>(data), encoded.size())};
    memcpy(data, encoded.data(), encoded.size()); *length = static_cast<unsigned int>(encoded.size());
    std::vector<short int> previous{42};
    EXPECT_FALSE(x_get_resource_i16("matrix.bin", &previous));
    EXPECT_EQ(previous, (std::vector<short int>{42}));
}

TEST(IncrementalSearchPlugin, InitializesAndMatchesJapanese)
{
    QPluginLoader loader(pluginPath("incrementalsearchplugin"));
    auto iface = qobject_cast<IncrementalSearchInterface *>(loader.instance());
    ASSERT_NE(iface, nullptr) << loader.errorString().toStdString();
    std::unique_ptr<IncrementalSearch> search(iface->create());
    ASSERT_TRUE(search->open());
    auto result = search->match("吾輩は猫である", search->makeFilter("neko"));
    ASSERT_TRUE(static_cast<bool>(result)); ASSERT_EQ(result.parts.size(), 3u);
    EXPECT_EQ(toQString(result.parts[1].text), "猫"); EXPECT_TRUE(result.parts[1].match);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
