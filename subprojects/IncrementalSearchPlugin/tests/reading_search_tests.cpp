#include <QDir>
#include <QPluginLoader>
#include <QTest>

#include <memory>

#include "IncrementalSearchInterface.h"

class ReadingSearchTests : public QObject {
	Q_OBJECT
private:
	QPluginLoader loader;
	std::unique_ptr<IncrementalSearch> searchPlugin;

	static void verifyResult(QString const &text, incrementalsearch::Result const &result,
		bool matches, QStringList const &highlights)
	{
		QCOMPARE(result.match, matches);
		// ハイライトの範囲に重複や欠落がなく、元の UTF-8 文字列を復元できること。
		std::string original = text.toStdString();
		std::string restored;
		QStringList actual_highlights;
		size_t cursor = 0;
		for (auto const &part : result.parts) {
			QCOMPARE(part.pos, cursor);
			QVERIFY(part.pos <= part.end);
			QVERIFY(part.end <= original.size());
			QCOMPARE(to_std_string(part.text), original.substr(part.pos, part.end - part.pos));
			restored += to_std_string(part.text);
			if (part.match) actual_highlights.push_back(toQString(part.text));
			cursor = part.end;
		}
		QCOMPARE(cursor, original.size());
		QCOMPARE(restored, original);
		QCOMPARE(actual_highlights, highlights);
	}

private slots:
	void initTestCase()
	{
		QString name = QStringLiteral("incrementalsearchplugin");
#ifdef QT_DEBUG
		name += 'd';
#endif
		QDir bin(QCoreApplication::applicationDirPath());
#ifdef Q_OS_WIN
		loader.setFileName(bin.filePath(name));
#else
		loader.setFileName(bin.filePath(QStringLiteral("../lib/soramimi.jp/lib") + name));
#endif
		auto *instance = loader.instance();
		QVERIFY2(instance, qPrintable(loader.errorString()));
		auto *factory = qobject_cast<IncrementalSearchInterface *>(instance);
		QVERIFY(factory);
		searchPlugin.reset(factory->create());
		QVERIFY(searchPlugin);
		QVERIFY(searchPlugin->open());
	}

	void readingMatch_data()
	{
		QTest::addColumn<QString>("text");
		QTest::addColumn<QString>("query");
		QTest::addColumn<QStringList>("highlights");
		QTest::addColumn<bool>("matches");
		QTest::newRow("Tokyo and prefecture suffix")
			<< QString("東京都.txt") << QString("toukyouto")
			<< QStringList{"東京都"} << true;
		QTest::newRow("whole sentence across five morphemes")
			<< QString("吾輩は猫である.txt") << QString("wagahaihanekodearu")
			<< QStringList{"吾輩は猫である"} << true;
		QTest::newRow("two morphemes")
			<< QString("吾輩は猫である.txt") << QString("wagahaiha")
			<< QStringList{"吾輩は"} << true;
		QTest::newRow("partial first and last readings")
			<< QString("吾輩は猫である.txt") << QString("haihane")
			<< QStringList{"吾輩は猫"} << true;
		QTest::newRow("suffix across three morphemes")
			<< QString("吾輩は猫である.txt") << QString("nekodearu")
			<< QStringList{"猫である"} << true;
		QTest::newRow("repeated occurrences")
			<< QString("吾輩は猫である、吾輩は猫である.txt") << QString("nekodearu")
			<< QStringList{"猫である", "猫である"} << true;
		QTest::newRow("overlapping occurrences")
			<< QString("吾輩は猫である吾輩は猫である吾輩は猫である.txt") << QString("aruwagahaihanekodearu")
			<< QStringList{"ある吾輩は猫である吾輩は猫である"} << true;
		QTest::newRow("source gap between morphemes")
			<< QString("吾輩 は猫である.txt") << QString("wagahaiha")
			<< QStringList{"吾輩 は"} << true;
		QTest::newRow("katakana original spelling")
			<< QString("カタカナの資料.txt") << QString("タ")
			<< QStringList{"タ"} << true;
		QTest::newRow("single morpheme")
			<< QString("吾輩は猫である.txt") << QString("neko")
			<< QStringList{"猫"} << true;
		QTest::newRow("case insensitive original spelling")
			<< QString("ReadMe.TXT") << QString("rEaDmE")
			<< QStringList{"ReadMe"} << true;
		QTest::newRow("no match")
			<< QString("吾輩は猫である.txt") << QString("inudehaaru")
			<< QStringList{} << false;
		QTest::newRow("empty filter")
			<< QString("吾輩は猫である.txt") << QString()
			<< QStringList{} << true;
		QTest::newRow("empty source")
			<< QString() << QString("neko")
			<< QStringList{} << false;
		QTest::newRow("name and extension")
			<< QString("東京都.txt") << QString("都.tx")
			<< QStringList{"都.tx"} << true;
	}

	void readingMatch()
	{
		QFETCH(QString, text);
		QFETCH(QString, query);
		QFETCH(QStringList, highlights);
		QFETCH(bool, matches);
		auto &search = *searchPlugin;
		auto filter = search.makeFilter(query.toStdString());
		auto result = search.match(fromQString(text), filter);
		verifyResult(text, result, matches, highlights);
	}

	void originalAndReadingMatchesDoNotDuplicateText()
	{
		IncrementalSearchFilter filter;
		// 通常のローマ字変換を介さず、公開フィルタの原表記と読みの一致範囲が
		// 重なる場合を作り、形態素全体に含まれる小さい一致の結合を検証する。
		filter.original_text = fromQString("タ");
		filter.katakana_text = fromQString("タ");
		const QString text = "カタカナの資料.txt";
		auto result = searchPlugin->match(fromQString(text), filter);
		verifyResult(text, result, true, {"カタカナ"});
	}

	void romajiConversion()
	{
		auto &search = *searchPlugin;
		QCOMPARE(QString::fromStdString(search.convertRomajiToKatakana("nn")), QString("ン"));
		QCOMPARE(QString::fromStdString(search.convertRomajiToKatakana("kona")), QString("コナ"));
		QCOMPARE(QString::fromStdString(search.convertRomajiToKatakana("konna")), QString("コンア"));
		QCOMPARE(QString::fromStdString(search.convertRomajiToKatakana("konnna")), QString("コンナ"));
		QCOMPARE(QString::fromStdString(search.convertRomajiToKatakana("konnnya")), QString("コンニャ"));
	}
};

QTEST_GUILESS_MAIN(ReadingSearchTests)
#include "reading_search_tests.moc"
