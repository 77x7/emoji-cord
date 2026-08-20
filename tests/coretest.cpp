// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appsettings.h"
#include "emojicatalog.h"
#include "emojimatcher.h"
#include "completioncontroller.h"
#include "contextrouter.h"
#include "querystate.h"
#include "usagestore.h"

#include <QFile>
#include <QSignalSpy>
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
    void controllerRestoresQueryFromSurroundingText();
    void controllerLoadsEverySuggestion();
    void demoInputRefinesAndRestartsQuery();
    void contextRouterFailsClosed();
    void contextRouterPrioritizesDirectInput();
    void fallbackExactCompletionIncludesClosingColon();
    void fallbackSelectionExcludesClosingColon();
    void settingsPersistAndValidateVisibleSuggestions();
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
    QCOMPARE(controller.candidates()->selectedIndex(), 0);
    controller.moveSelection(1);
    QCOMPARE(controller.candidates()->selectedIndex(), 1);
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

void CoreTest::controllerRestoresQueryFromSurroundingText()
{
    WaylandInputMethod inputMethod;
    CompletionController controller(&inputMethod);
    QString error;
    QVERIFY2(controller.loadCatalog(catalogJson, &error), qPrintable(error));

    const QString text = QStringLiteral("emoji 💀 :sk");
    const std::uint32_t cursor = std::uint32_t(text.toUtf8().size());
    emit inputMethod.resetRequested();
    emit inputMethod.surroundingTextChanged(text, cursor, cursor);

    QCOMPARE(controller.query(), QStringLiteral("sk"));
    QVERIFY(controller.isVisible());
    QCOMPARE(controller.candidates()->rowCount(), 3);

    emit inputMethod.surroundingTextChanged(text, cursor, cursor - 1);
    QVERIFY(controller.query().isEmpty());
    QVERIFY(!controller.isVisible());
}

void CoreTest::controllerLoadsEverySuggestion()
{
    CompletionController controller(nullptr);
    QByteArray largeCatalog("[");
    for (int index = 0; index < 100; ++index) {
        if (index > 0) {
            largeCatalog.append(',');
        }
        largeCatalog.append(QStringLiteral("{\"alias\":\"match%1\",\"emoji\":\"x\"}")
                                .arg(index)
                                .toUtf8());
    }
    largeCatalog.append(']');
    QString error;
    QVERIFY2(controller.loadCatalog(largeCatalog, &error), qPrintable(error));
    controller.preview(QStringLiteral("match"));
    QCOMPARE(controller.candidates()->rowCount(), 100);
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

void CoreTest::contextRouterFailsClosed()
{
    ContextRouter router;
    QSignalSpy applicationChanges(&router, &ContextRouter::activeApplicationChanged);
    QCOMPARE(router.route(), ContextRouter::Route::None);

    router.setFallbackEnabled(true);
    router.activeWindowChanged(QStringLiteral("window-1"), QStringLiteral("org.kde.kate"),
        QStringLiteral("kate"), QStringLiteral("kate"), true);
    QCOMPARE(router.route(), ContextRouter::Route::None);
    QCOMPARE(applicationChanges.count(), 1);
    router.activeWindowChanged(QStringLiteral("window-1"), QStringLiteral("org.kde.kate"),
        QStringLiteral("kate"), QStringLiteral("kate"), true);
    QCOMPARE(applicationChanges.count(), 1);

    router.activeWindowChanged(QStringLiteral("window-2"),
        QStringLiteral("com.valvesoftware.Steam.desktop"), QString(), QString(), true);
    QCOMPARE(router.route(), ContextRouter::Route::None);

    router.setSessionLocked(false);
    QCOMPARE(router.route(), ContextRouter::Route::Fallback);

    router.setSessionLocked(true);
    QCOMPARE(router.route(), ContextRouter::Route::None);

    router.setSessionLocked(false);
    QCOMPARE(router.route(), ContextRouter::Route::Fallback);
    router.serviceOwnerChanged(QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral(":1.1"), QString());
    QCOMPARE(router.route(), ContextRouter::Route::None);

    router.activeWindowChanged(QString(), QString(), QString(), QString(), false);
    QCOMPARE(router.route(), ContextRouter::Route::None);
}

void CoreTest::contextRouterPrioritizesDirectInput()
{
    ContextRouter router;
    router.setFallbackEnabled(true);
    router.setSessionLocked(false);
    router.activeWindowChanged(QStringLiteral("window-1"), QStringLiteral("steam"),
        QString(), QString(), true);
    QCOMPARE(router.route(), ContextRouter::Route::Fallback);

    router.setDirectContextActive(true);
    QCOMPARE(router.route(), ContextRouter::Route::Direct);

    router.activeWindowChanged(QStringLiteral("window-2"), QStringLiteral("org.kde.kate"),
        QString(), QString(), true);
    QCOMPARE(router.route(), ContextRouter::Route::Direct);

    router.setDirectContextActive(false);
    QCOMPARE(router.route(), ContextRouter::Route::None);

    router.activeWindowChanged(QStringLiteral("window-3"), QStringLiteral("steam"),
        QString(), QString(), true);
    QCOMPARE(router.route(), ContextRouter::Route::Fallback);
    router.serviceOwnerChanged(QStringLiteral("org.kde.KWin"),
        QStringLiteral(":1.2"), QString());
    QCOMPARE(router.route(), ContextRouter::Route::None);
}

void CoreTest::fallbackExactCompletionIncludesClosingColon()
{
    CompletionController controller(nullptr);
    QString error;
    QVERIFY2(controller.loadCatalog(catalogJson, &error), qPrintable(error));
    controller.setFallbackMode(true);
    QSignalSpy commits(&controller, &CompletionController::fallbackCommitRequested);
    QSignalSpy confirmed(&controller, &CompletionController::committed);

    for (const QChar character : QStringLiteral(":skull:")) {
        controller.observeFallbackCharacter(character);
    }

    QCOMPARE(commits.count(), 1);
    QCOMPARE(commits.first().at(0).toInt(), 7);
    QCOMPARE(commits.first().at(1).toString(), QStringLiteral("💀"));
    QCOMPARE(confirmed.count(), 0);
    controller.confirmFallbackCommit(commits.first().at(1).toString(),
        commits.first().at(2).toString());
    QCOMPARE(confirmed.count(), 1);
    QVERIFY(!controller.isVisible());
}

void CoreTest::fallbackSelectionExcludesClosingColon()
{
    CompletionController controller(nullptr);
    QString error;
    QVERIFY2(controller.loadCatalog(catalogJson, &error), qPrintable(error));
    controller.setFallbackMode(true);
    QSignalSpy commits(&controller, &CompletionController::fallbackCommitRequested);

    for (const QChar character : QStringLiteral(":sk")) {
        controller.observeFallbackCharacter(character);
    }
    const QString selectedEmoji = controller.candidates()->selectedEntry()->emoji;
    controller.select(0);

    QCOMPARE(commits.count(), 1);
    QCOMPARE(commits.first().at(0).toInt(), 3);
    QCOMPARE(commits.first().at(1).toString(), selectedEmoji);
}

void CoreTest::settingsPersistAndValidateVisibleSuggestions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("nested/settings.json"));

    AppSettings settings(path);
    QCOMPARE(settings.visibleSuggestions(), AppSettings::defaultVisibleSuggestions);
    QCOMPARE(settings.backgroundOpacity(), AppSettings::automaticBackgroundOpacity);
    QVERIFY(settings.blurEnabled());
    QVERIFY(settings.contrastEnabled());
    QVERIFY(!settings.dynamicWidth());
    QCOMPARE(settings.pickerWidth(), AppSettings::defaultPickerWidth);
    QCOMPARE(settings.maximumPickerWidth(), AppSettings::defaultMaximumPickerWidth);
    QVERIFY(settings.updateVisibleSuggestions(17));
    QVERIFY(settings.updateBackgroundOpacity(65));
    QVERIFY(settings.updateBlurEnabled(false));
    QVERIFY(settings.updateContrastEnabled(false));
    QVERIFY(settings.updateDynamicWidth(true));
    QVERIFY(settings.updatePickerWidth(360));
    QVERIFY(settings.updateMaximumPickerWidth(640));
    QCOMPARE(settings.visibleSuggestions(), 17);
    QCOMPARE(settings.backgroundOpacity(), 65);
    QVERIFY(!settings.blurEnabled());
    QVERIFY(!settings.contrastEnabled());
    QVERIFY(settings.dynamicWidth());
    QCOMPARE(settings.pickerWidth(), 360);
    QCOMPARE(settings.maximumPickerWidth(), 640);
    QVERIFY(!settings.updateVisibleSuggestions(0));
    QVERIFY(!settings.updateBackgroundOpacity(19));
    QVERIFY(!settings.updatePickerWidth(219));
    QVERIFY(!settings.updateMaximumPickerWidth(2001));
    QCOMPARE(settings.visibleSuggestions(), 17);
    QCOMPARE(settings.backgroundOpacity(), 65);

    AppSettings restored(path);
    QCOMPARE(restored.visibleSuggestions(), 17);
    QCOMPARE(restored.backgroundOpacity(), 65);
    QVERIFY(!restored.blurEnabled());
    QVERIFY(!restored.contrastEnabled());
    QVERIFY(restored.dynamicWidth());
    QCOMPARE(restored.pickerWidth(), 360);
    QCOMPARE(restored.maximumPickerWidth(), 640);
    QVERIFY(restored.error().isEmpty());

    QVERIFY(restored.updateBackgroundOpacity(0));
    QCOMPARE(restored.backgroundOpacity(), AppSettings::automaticBackgroundOpacity);
    QVERIFY(restored.updateBackgroundOpacity(20));
    QVERIFY(restored.updateBackgroundOpacity(100));
    QVERIFY(restored.updateBackgroundOpacity(70));

    AppSettings concurrent(path);
    QVERIFY(concurrent.updateBlurEnabled(true));
    AppSettings merged(path);
    QCOMPARE(merged.backgroundOpacity(), 70);
    QVERIFY(merged.blurEnabled());
    QVERIFY(merged.applyBackgroundOpacity(80));
    AppSettings unchangedOnDisk(path);
    QCOMPARE(unchangedOnDisk.backgroundOpacity(), 70);

    QFile malformed(path);
    QVERIFY(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(malformed.write("{\"maxSuggestions\": -2}"), qint64(22));
    malformed.close();
    AppSettings rejected(path);
    QCOMPARE(rejected.visibleSuggestions(), AppSettings::defaultVisibleSuggestions);
    QVERIFY(!rejected.error().isEmpty());

    const QString appearanceFirstPath = directory.filePath(
        QStringLiteral("appearance-first/settings.json"));
    AppSettings appearanceFirst(appearanceFirstPath);
    QVERIFY(appearanceFirst.updateBackgroundOpacity(55));
    AppSettings appearanceFirstRestored(appearanceFirstPath);
    QCOMPARE(appearanceFirstRestored.visibleSuggestions(),
        AppSettings::defaultVisibleSuggestions);
    QCOMPARE(appearanceFirstRestored.backgroundOpacity(), 55);
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
