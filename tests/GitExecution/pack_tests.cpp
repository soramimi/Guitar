#include <gtest/gtest.h>
#include "../../src/GitPack.h"
#include "../../src/GitPackIdxV2.h"
#include "../../src/GitObjectManager.h"
#include "../../src/Git.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <zlib.h>

namespace {
void be32(QByteArray &data, uint32_t n) { for (int i = 3; i >= 0; --i) data += char(n >> (i * 8)); }
QByteArray compressed(QByteArray const &data) {
    uLongf size = compressBound(data.size()); QByteArray out(size, 0);
    EXPECT_EQ(compress(reinterpret_cast<Bytef *>(out.data()), &size,
        reinterpret_cast<Bytef const *>(data.constData()), data.size()), Z_OK);
    out.resize(size); return out;
}
QByteArray entry(unsigned type, QByteArray const &content, QByteArray const &ref = {}) {
    uint64_t size = content.size(); QByteArray out;
    unsigned c = (type << 4) | (size & 15); size >>= 4;
    out += char(c | (size ? 128 : 0));
    while (size) { c = size & 127; size >>= 7; out += char(c | (size ? 128 : 0)); }
    return out + ref + compressed(content);
}
uint32_t crc(QByteArray const &data) { return crc32(0, reinterpret_cast<Bytef const *>(data.constData()), data.size()); }
class PackedObjects : public ::testing::Test {
protected:
    QTemporaryDir temp;
    GitRunner runner;
    QByteArray oid = QByteArray(20, 1);
    void SetUp() override {
        ASSERT_TRUE(temp.isValid()); ASSERT_TRUE(QDir().mkpath(temp.path() + "/.git/objects/pack"));
        GitContext context; context.git_command = QStandardPaths::findExecutable("git").toStdString();
        runner = GitRunner(std::make_shared<Git>(context, temp.path().toStdString(), "", ""));
    }
    void save(QByteArray const &packed, uint64_t offset = 12, uint32_t marker = 0x80000000u, bool badCrc = false) {
        auto dir = temp.path() + "/.git/objects/pack/pack-test";
        QFile pack(dir + ".pack"); ASSERT_TRUE(pack.open(QIODevice::WriteOnly));
        QByteArray header("PACK"); be32(header, 2); be32(header, 1);
        ASSERT_EQ(pack.write(header), header.size()); ASSERT_TRUE(pack.seek(qint64(offset)));
        ASSERT_EQ(pack.write(packed), packed.size()); ASSERT_EQ(pack.write(QByteArray(20, 0)), 20); pack.close();
        QByteArray idx = QByteArray::fromHex("ff744f6300000002");
        for (int i = 0; i < 256; ++i) be32(idx, i < 1 ? 0 : 1);
        idx += oid; be32(idx, crc(packed) ^ (badCrc ? 1 : 0));
        be32(idx, offset < 0x80000000u ? uint32_t(offset) : marker);
        if (offset >= 0x80000000u) { be32(idx, uint32_t(offset >> 32)); be32(idx, uint32_t(offset)); }
        idx += QByteArray(20, 0); idx += QCryptographicHash::hash(idx, QCryptographicHash::Sha1);
        QFile index(dir + ".idx"); ASSERT_TRUE(index.open(QIODevice::WriteOnly)); ASSERT_EQ(index.write(idx), idx.size());
    }
    bool load(QByteArray *data, GitObject::Type *type) {
        GitObjectManager manager(nullptr); return manager.catFile(runner, GitHash(oid.toHex().toStdString()), data, type);
    }
};
}
TEST(PackDelta, RejectsTruncationOverflowInvalidSizesAndOpcodes) {
    for (auto hex : {"", "01", "0101ff", "01019fffffffff01", "0105027879", "0200", "010100", "01020378", "ffffffffffffffffffff7f00"})
        EXPECT_FALSE(GitPack::applyDelta("x", QByteArray::fromHex(hex)).has_value()) << hex;
    auto empty = GitPack::applyDelta("x", QByteArray::fromHex("0100")); ASSERT_TRUE(empty); EXPECT_TRUE(empty->isEmpty());
    auto insert = GitPack::applyDelta("x", QByteArray::fromHex("0102027879")); ASSERT_TRUE(insert); EXPECT_EQ(*insert, "xy");
    auto copy = GitPack::applyDelta("abc", QByteArray::fromHex("0303910003")); ASSERT_TRUE(copy); EXPECT_EQ(*copy, "abc");
    auto big = GitPack::applyDelta(QByteArray(65536, 'x'), QByteArray::fromHex("80800480800480"));
    ASSERT_TRUE(big); EXPECT_EQ(big->size(), 65536);
}
TEST(PackInflate, ExactSizesErrorsAndTransactionalOutput) {
    for (QByteArray const &source : {QByteArray(), QByteArray(100, 'x'), QByteArray(200000, 'a')}) {
        auto zipped = compressed(source); QBuffer buffer(&zipped); ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
        QByteArray out("unchanged"); size_t consumed = 0;
        ASSERT_TRUE(GitPack::decompress(&buffer, source.size(), &out, &consumed, nullptr, true));
        EXPECT_EQ(out, source); EXPECT_EQ(consumed, size_t(zipped.size()));
        buffer.seek(0); out = "unchanged";
        EXPECT_FALSE(GitPack::decompress(&buffer, source.size() + 1, &out, nullptr, nullptr, true)); EXPECT_EQ(out, "unchanged");
        if (!source.isEmpty()) { buffer.seek(0); EXPECT_FALSE(GitPack::decompress(&buffer, 1, &out, nullptr, nullptr, true)); }
        zipped.chop(1); buffer.seek(0); EXPECT_FALSE(GitPack::decompress(&buffer, source.size(), &out, nullptr, nullptr, true));
    }
    class FailedRead : public QIODevice { qint64 readData(char *, qint64) override { return -1; } qint64 writeData(char const *, qint64) override { return -1; } } bad;
    bad.open(QIODevice::ReadOnly); QByteArray out; EXPECT_FALSE(GitPack::decompress(&bad, 100, &out));
}
TEST(PackHeader, WideSizesAndMalformedVarints) {
    GitPackIdxItem item{}; item.offset = 12;
    for (auto hex : {"b080808040", "b08080808080808080808000", "50", "b080"}) {
        QByteArray data = QByteArray::fromHex("5041434b0000000200000001") + QByteArray::fromHex(hex);
        QBuffer buffer(&data); ASSERT_TRUE(buffer.open(QIODevice::ReadOnly)); GitPackInfo info;
        bool ok = GitPack::seekPackedObject(&buffer, &item, &info);
        if (QByteArray(hex) == "b080808040") { ASSERT_TRUE(ok); EXPECT_EQ(info.expanded_size, size_t(2147483648ULL)); }
        else EXPECT_FALSE(ok) << hex;
    }
}
TEST_F(PackedObjects, LoadsEmptyAndNonemptyBlobsAndRejectsBadCrc) {
    for (QByteArray data : {QByteArray(), QByteArray("hello")}) {
        save(entry(3, data)); QByteArray out; GitObject::Type type;
        ASSERT_TRUE(load(&out, &type)); EXPECT_EQ(out, data); EXPECT_EQ(type, GitObject::Type::BLOB);
    }
    save(entry(3, "hello"), 12, 0x80000000u, true); QByteArray out; GitObject::Type type; EXPECT_FALSE(load(&out, &type));
}
TEST_F(PackedObjects, ResolvesLargeOffsetsAndRejectsInvalidTableIndex) {
    if (sizeof(size_t) < 8) GTEST_SKIP() << "64-bit offset test";
    save(entry(3, "large offset"), 0x100000010ULL); QByteArray out; GitObject::Type type;
    ASSERT_TRUE(load(&out, &type)); EXPECT_EQ(out, "large offset");
    save(entry(3, "large offset"), 0x100000010ULL, 0x80000001u); EXPECT_FALSE(load(&out, &type));
}
TEST_F(PackedObjects, RejectsMissingAndCyclicDeltaBases) {
    QByteArray out; GitObject::Type type;
    save(entry(7, QByteArray::fromHex("0000"), oid)); EXPECT_FALSE(load(&out, &type));
    save(entry(7, QByteArray::fromHex("0000"), QByteArray(20, 2))); EXPECT_FALSE(load(&out, &type));
    save(entry(6, QByteArray::fromHex("0000"), QByteArray(1, 0))); EXPECT_FALSE(load(&out, &type));
}
TEST(PackTree, GitlinksAndQuotedPathsRoundTrip) {
    QByteArray binary;
    std::vector<std::string> names = {"submodule", "line\nbreak", "tab\tname", " leading ", "quote\"slash\\", "日本語"};
    for (auto const &name : names) { binary += "160000 "; binary += QByteArray::fromStdString(name); binary += '\0'; binary += QByteArray(20, 1); }
    ASSERT_TRUE(GitPack::decodeTree(&binary)); auto parsed = parseGitTreeObject(binary, ""); ASSERT_EQ(parsed.size(), names.size());
    for (size_t i = 0; i < names.size(); ++i) { EXPECT_EQ(parsed[i].name, names[i]); EXPECT_EQ(parsed[i].type, GitTreeItem::COMMIT); }
    QByteArray broken("100644 missing"); EXPECT_FALSE(GitPack::decodeTree(&broken)); EXPECT_EQ(broken, "100644 missing");
}
TEST(PackLooseHeader, ValidatesTypeLengthAndTruncation) {
    QByteArray good("blob 3\0abc", 10); EXPECT_EQ(GitPack::stripHeader(&good), GitObject::Type::BLOB); EXPECT_EQ(good, "abc");
    for (auto invalid : {QByteArray("blob 4\0abc", 10), QByteArray("blob x\0abc", 10), QByteArray("blob"), QByteArray("blob 9999999999999999999999999")})
        EXPECT_EQ(GitPack::stripHeader(&invalid), GitObject::Type::UNKNOWN);
}
TEST_F(PackedObjects, MatchesGitForRealOffsetAndReferenceDeltaPacks) {
    auto run = [&](QStringList args, QByteArray input = {}) {
        QProcess process; process.setWorkingDirectory(temp.path());
        process.start(QStandardPaths::findExecutable("git"), args);
        if (!process.waitForStarted()) { ADD_FAILURE() << "git start failed"; return QByteArray(); }
        process.write(input); process.closeWriteChannel();
        if (!process.waitForFinished(30000) || process.exitCode() != 0)
            ADD_FAILURE() << process.readAllStandardError().constData();
        return process.readAllStandardOutput();
    };
    run({"init", "-q"});
    std::vector<std::pair<QByteArray, QByteArray>> objects;
    QByteArray ids, original;
    for (int i = 0; i < 2000; ++i) original += QByteArray::number(i) + " unique test line\n";
    for (int i = 0; i < 12; ++i) {
        auto content = original + QByteArray::number(i) + " tail\n";
        auto id = run({"hash-object", "-w", "--stdin"}, content).trimmed(); ASSERT_EQ(id.size(), 40);
        objects.emplace_back(id, content); ids += id + '\n';
    }
    for (bool ofs : {true, false}) {
        QStringList args{"pack-objects", "--stdout", "--window=50", "--depth=20"};
        if (ofs) args += "--delta-base-offset";
        auto pack = run(args, ids); ASSERT_TRUE(pack.startsWith("PACK"));
        auto path = temp.path() + "/.git/objects/pack/pack-real.pack";
        QFile file(path); ASSERT_TRUE(file.open(QIODevice::WriteOnly)); ASSERT_EQ(file.write(pack), pack.size()); file.close();
        run({"index-pack", "--index-version=2", path});
        auto verification = run({"verify-pack", "-v", path.left(path.size() - 4) + "idx"});
        EXPECT_TRUE(verification.contains("chain length")) << verification.constData();
        for (auto const &[id, content] : objects) {
            auto loose = temp.path() + "/.git/objects/" + QString::fromLatin1(id.left(2)) + '/' + QString::fromLatin1(id.mid(2));
            if (QFile::exists(loose)) { ASSERT_TRUE(QFile::remove(loose)); }
        }
        GitObjectManager manager(nullptr);
        for (auto const &[id, content] : objects) {
            QByteArray out; GitObject::Type type;
            ASSERT_TRUE(manager.catFile(runner, GitHash(id.toStdString()), &out, &type));
            EXPECT_EQ(type, GitObject::Type::BLOB); EXPECT_EQ(out, content);
            EXPECT_EQ(out, run({"cat-file", "-p", QString::fromLatin1(id)}));
        }
    }
}
