// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "emojicatalog.h"
#include "emojimatcher.h"
#include "completioncontroller.h"
#include "querystate.h"
#include "usagestore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <utility>

class CoreTest : public QObject
{
    Q_OBJECT

private slots:
    void catalogLoadsAndNormalizes();
    void catalogRejectsDuplicatesWithoutReplacingData();
    void catalogLoadsLocalTsvVocabulary();
    void queryTracksShortcode();
    void queryCancelsOnUnsupportedInput();
    void matcherUsesQualityTiers();
    void matcherUsesFrequencyAndRecency();
    void matcherHandlesTransposition();
    void controllerBuildsVerticalCandidates();
    void controllerConsumesNavigationAndExactCompletion();
    void controllerRetainsConsumedReleaseAcrossSelection();
    void demoInputRefinesAndRestartsQuery();
    void usageRoundTripsAtomically();
};

namespace {
const QByteArray catalogJson = R"json(
[
  {"alias":"skull","emoji":"💀","aliases":["death"],"keywords":["dead","skeleton"]},
  {"alias":"skull_and_crossbones","emoji":"☠️","aliases":[],"keywords":["pirate","danger"]},
  {"alias":"skier","emoji":"⛷️","aliases":[],"keywords":["sport","snow"]},
  {"alias":"thinking","emoji":"🤔","aliases":["think"],"keywords":["hmm","face"]}
]
)json";

EmojiCatalog makeCatalog()
{
    EmojiCatalog catalog;
    QString error;
    if (!catalog.loadJson(catalogJson, &error)) {
        qFatal("Test catalog failed to load: %s", qPrintable(error));
    }
    return catalog;
}
}

void CoreTest::catalogLoadsAndNormalizes()
{
    EmojiCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadJson(catalogJson, &error), qPrintable(error));
    QCOMPARE(catalog.entries().size(), 4);
    QCOMPARE(catalog.findExact(u"SKULL")->emoji, QStringLiteral("💀"));
    QCOMPARE(catalog.findExact(u"missing"), nullptr);
}

void CoreTest::catalogRejectsDuplicatesWithoutReplacingData()
{
    EmojiCatalog catalog = makeCatalog();
    QString error;
    QVERIFY(!catalog.loadJson(R"([{"alias":"x","emoji":"1"},{"alias":"X","emoji":"2"}])", &error));
    QVERIFY(error.contains(QStringLiteral("Duplicate")));
    QCOMPARE(catalog.entries().size(), 4);
}

void CoreTest::catalogLoadsLocalTsvVocabulary()
{
    EmojiCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadTsv("face\t😀\nfire\t🔥\nflag_white\t🏳️\n", &error), qPrintable(error));
    QCOMPARE(catalog.entries().size(), 3);
    QCOMPARE(catalog.findExact(u"fire")->emoji, QStringLiteral("🔥"));

    const UsageStore usage;
    const QVector<EmojiMatch> matches = EmojiMatcher::match(catalog, u"f", usage);
    QCOMPARE(matches.size(), 3);
    QCOMPARE(matches.first().tier, MatchTier::Prefix);
}

void CoreTest::queryTracksShortcode()
{
    QueryState state;
    QCOMPARE(state.input(u':'), QueryState::Change::Armed);
    QCOMPARE(state.input(u'S'), QueryState::Change::Updated);
    QCOMPARE(state.input(u'k'), QueryState::Change::Updated);
    QCOMPARE(state.query(), u"sk");
    QCOMPARE(state.shortcodeLength(), 3);
    QCOMPARE(state.input(u':'), QueryState::Change::ExactRequested);
    QCOMPARE(state.shortcodeLength(true), 4);
    QCOMPARE(state.backspace(), QueryState::Change::Updated);
    QCOMPARE(state.query(), u"s");
}

void CoreTest::queryCancelsOnUnsupportedInput()
{
    QueryState state;
    QCOMPARE(state.input(u'x'), QueryState::Change::Ignored);
    state.input(u':');
    state.input(u's');
    QCOMPARE(state.input(u' '), QueryState::Change::Cancelled);
    QVERIFY(!state.isArmed());
    QVERIFY(state.query().isEmpty());
}

void CoreTest::matcherUsesQualityTiers()
{
    const EmojiCatalog catalog = makeCatalog();
    const UsageStore usage;

    const QVector<EmojiMatch> exact = EmojiMatcher::match(catalog, u"death", usage);
    QCOMPARE(exact.first().entry->alias, QStringLiteral("skull"));
    QCOMPARE(exact.first().tier, MatchTier::Exact);

    const QVector<EmojiMatch> prefix = EmojiMatcher::match(catalog, u"sk", usage);
    QCOMPARE(prefix.size(), 3);
    QCOMPARE(prefix.at(0).tier, MatchTier::Prefix);

    const QVector<EmojiMatch> keyword = EmojiMatcher::match(catalog, u"pirate", usage);
    QCOMPARE(keyword.first().entry->alias, QStringLiteral("skull_and_crossbones"));
    QCOMPARE(keyword.first().tier, MatchTier::Substring);
}

void CoreTest::matcherUsesFrequencyAndRecency()
{
    const EmojiCatalog catalog = makeCatalog();
    UsageStore usage;
    usage.record(u"skier");
    usage.record(u"skull");
    usage.record(u"skier");

    QVector<EmojiMatch> matches = EmojiMatcher::match(catalog, u"sk", usage);
    QCOMPARE(matches.at(0).entry->alias, QStringLiteral("skier"));
    QCOMPARE(matches.at(1).entry->alias, QStringLiteral("skull"));

    usage.record(u"skull");
    matches = EmojiMatcher::match(catalog, u"sk", usage);
    QCOMPARE(matches.at(0).entry->alias, QStringLiteral("skull"));
}

void CoreTest::matcherHandlesTransposition()
{
    const EmojiCatalog catalog = makeCatalog();
    const UsageStore usage;
    const QVector<EmojiMatch> matches = EmojiMatcher::match(catalog, u"sklul", usage);
    QCOMPARE(matches.first().entry->alias, QStringLiteral("skull"));
    QCOMPARE(matches.first().tier, MatchTier::Fuzzy);
    QCOMPARE(matches.first().editDistance, 1);
}

void CoreTest::controllerBuildsVerticalCandidates()
{
    CompletionController controller(nullptr);
    QString error;
    QVERIFY2(controller.loadCatalog(catalogJson, &error), qPrintable(error));
    controller.preview(QStringLiteral("sk"));

    QVERIFY(controller.isVisible());
    QCOMPARE(controller.query(), QStringLiteral("sk"));
    QCOMPARE(controller.candidates()->rowCount(), 3);
    QCOMPARE(controller.candidates()->selectedIndex(), 0);

    controller.moveSelection(-1);
    QCOMPARE(controller.candidates()->selectedIndex(), 2);
    controller.moveSelection(1);
    QCOMPARE(controller.candidates()->selectedIndex(), 0);
    controller.dismiss();
    QVERIFY(!controller.isVisible());
}

void CoreTest::controllerConsumesNavigationAndExactCompletion()
{
    CompletionController controller(nullptr);
    QString error;
    QVERIFY2(controller.loadCatalog(catalogJson, &error), qPrintable(error));

    auto press = [&controller](std::uint32_t keycode, std::uint32_t keysym, QString text = {}) {
        WaylandInputMethod::KeyEvent event;
        event.keycode = keycode;
        event.keysym = keysym;
        event.text = std::move(text);
        event.pressed = true;
        return controller.handleKey(event);
    };
    auto release = [&controller](std::uint32_t keycode, std::uint32_t keysym) {
        WaylandInputMethod::KeyEvent event;
        event.keycode = keycode;
        event.keysym = keysym;
        event.pressed = false;
        return controller.handleKey(event);
    };

    QVERIFY(!press(39, ':', QStringLiteral(":")));
    for (const QChar character : QStringLiteral("skull")) {
        QVERIFY(!press(std::uint32_t(character.unicode()), character.unicode(), QString(character)));
    }
    QVERIFY(controller.isVisible());

    QVERIFY(press(108, XKB_KEY_Down));
    QVERIFY(release(108, XKB_KEY_Down));
    QVERIFY(press(39, ':', QStringLiteral(":")));
    QVERIFY(release(39, ':'));
    QVERIFY(!controller.isVisible());
    QVERIFY(controller.query().isEmpty());
}

void CoreTest::controllerRetainsConsumedReleaseAcrossSelection()
{
    CompletionController controller(nullptr);
    QString error;
    QVERIFY2(controller.loadCatalog(catalogJson, &error), qPrintable(error));
    controller.preview(QStringLiteral("sk"));

    WaylandInputMethod::KeyEvent down;
    down.keycode = 108;
    down.keysym = XKB_KEY_Down;
    down.pressed = true;
    QVERIFY(controller.handleKey(down));

    controller.select();
    QVERIFY(!controller.isVisible());

    down.pressed = false;
    QVERIFY(controller.handleKey(down));
    QVERIFY(!controller.handleKey(down));
}

void CoreTest::demoInputRefinesAndRestartsQuery()
{
    CompletionController controller(nullptr);
    QString error;
    QVERIFY2(controller.loadCatalog(catalogJson, &error), qPrintable(error));
    controller.preview(QStringLiteral("s"));

    controller.demoInput(QStringLiteral("k"));
    QCOMPARE(controller.query(), QStringLiteral("sk"));
    QCOMPARE(controller.candidates()->rowCount(), 3);

    controller.demoInput(QStringLiteral(":"));
    QCOMPARE(controller.query(), QString());
    controller.demoInput(QStringLiteral("thi"));
    QCOMPARE(controller.query(), QStringLiteral("thi"));
    QCOMPARE(controller.candidates()->rowCount(), 1);
    QCOMPARE(controller.candidates()->entryAt(0)->alias, QStringLiteral("thinking"));
}

void CoreTest::usageRoundTripsAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("nested/usage.json"));

    UsageStore saved;
    saved.record(u"skull");
    saved.record(u"thinking");
    saved.record(u"skull");
    QString error;
    QVERIFY2(saved.save(path, &error), qPrintable(error));

    UsageStore loaded;
    QVERIFY2(loaded.load(path, &error), qPrintable(error));
    QCOMPARE(loaded.usage(u"skull"), (EmojiUsage{2, 3}));
    QCOMPARE(loaded.usage(u"thinking"), (EmojiUsage{1, 2}));

    loaded.record(u"thinking");
    QCOMPARE(loaded.usage(u"thinking"), (EmojiUsage{2, 4}));
}

QTEST_MAIN(CoreTest)

#include "coretest.moc"
