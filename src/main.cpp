// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appsettings.h"
#include "completioncontroller.h"
#include "contextrouter.h"
#include "effectcapabilities.h"
#include "pickerwindow.h"
#include "waylandinputmethod.h"
#include "xwaylandfallback.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QSize>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QtGui/qguiapplication_platform.h>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Emoji-cord"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("System-wide emoji completion for KDE"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption settingsOption(QStringLiteral("settings"),
        QStringLiteral("Open the settings window."));
    const QCommandLineOption demoOption(QStringLiteral("demo-picker"),
        QStringLiteral("Open a standalone picker preview."));
    const QCommandLineOption disableFallbackOption(QStringLiteral("disable-xwayland-fallback"),
        QStringLiteral("Disable the Steam XWayland fallback."));
    parser.addOptions({settingsOption, demoOption, disableFallbackOption});
    parser.process(application);

    AppSettings settings(AppSettings::defaultPath());
    EffectCapabilities effectCapabilities;
    if (parser.isSet(settingsOption)) {
        QGuiApplication::setQuitOnLastWindowClosed(true);
        const auto updateRunningInstance = [&settings](const QString &method,
                                               const QVariant &value) {
                QDBusInterface remote(QStringLiteral("io.github.puzll.EmojiCord"),
                    QStringLiteral("/Settings"),
                    QStringLiteral("io.github.puzll.EmojiCord.Settings"),
                    QDBusConnection::sessionBus());
                if (!remote.isValid()) {
                    settings.setStatus(QStringLiteral(
                        "Saved. It will apply when Emoji-cord starts."));
                    return;
                }
                const QDBusReply<bool> reply = remote.call(
                    method, value);
                settings.setStatus(reply.isValid() && reply.value()
                        ? QStringLiteral("Saved and applied.")
                        : QStringLiteral(
                            "Saved, but the running input method could not be updated."));
            };
        QObject::connect(&settings, &AppSettings::visibleSuggestionsChanged,
            &application, [updateRunningInstance](int value) {
                updateRunningInstance(QStringLiteral("applyVisibleSuggestions"), value);
            });
        QObject::connect(&settings, &AppSettings::backgroundOpacityChanged,
            &application, [updateRunningInstance](int value) {
                updateRunningInstance(QStringLiteral("applyBackgroundOpacity"), value);
            });
        QObject::connect(&settings, &AppSettings::blurEnabledChanged,
            &application, [updateRunningInstance](bool enabled) {
                updateRunningInstance(QStringLiteral("applyBlurEnabled"), enabled);
            });
        QObject::connect(&settings, &AppSettings::contrastEnabledChanged,
            &application, [updateRunningInstance](bool enabled) {
                updateRunningInstance(QStringLiteral("applyContrastEnabled"), enabled);
            });
        QObject::connect(&settings, &AppSettings::dynamicWidthChanged,
            &application, [updateRunningInstance](bool enabled) {
                updateRunningInstance(QStringLiteral("applyDynamicWidth"), enabled);
            });
        QObject::connect(&settings, &AppSettings::pickerWidthChanged,
            &application, [updateRunningInstance](int value) {
                updateRunningInstance(QStringLiteral("applyPickerWidth"), value);
            });
        QObject::connect(&settings, &AppSettings::maximumPickerWidthChanged,
            &application, [updateRunningInstance](int value) {
                updateRunningInstance(QStringLiteral("applyMaximumPickerWidth"), value);
            });

        QQuickView view;
        view.setTitle(QStringLiteral("Emoji-cord Settings"));
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.rootContext()->setContextProperty(QStringLiteral("appSettings"), &settings);
        view.rootContext()->setContextProperty(QStringLiteral("effectCapabilities"),
            &effectCapabilities);
        view.setSource(QUrl(QStringLiteral("qrc:/qml/Settings.qml")));
        if (view.status() == QQuickView::Error) {
            QTextStream(stderr) << "Emoji-cord: could not load the settings window\n";
            return 1;
        }
        view.setMinimumSize(QSize(500, 360));
        view.resize(680, 660);
        view.show();
        return application.exec();
    }

    QGuiApplication::setQuitOnLastWindowClosed(false);

    const bool demoMode = parser.isSet(demoOption);
    const QString localCatalog = QDir::home().filePath(
        QStringLiteral(".local/share/fcitx5/emojicomplete/emoji.tsv"));
    const bool useLocalCatalog = QFileInfo::exists(localCatalog);
    QFile catalogFile(useLocalCatalog ? localCatalog : QStringLiteral(":/data/emoji-demo.json"));
    if (!catalogFile.open(QIODevice::ReadOnly)) {
        QTextStream(stderr) << "Emoji-cord: cannot open the development catalog\n";
        return 1;
    }

    WaylandInputMethod inputMethod;
    ContextRouter contextRouter;
    XWaylandFallback xwaylandFallback;
    CompletionController controller(demoMode ? nullptr : &inputMethod);
    QTimer pointerFallbackTimer;
    pointerFallbackTimer.setInterval(100);
    QPoint lastNoCaretClick;
    bool lastNoCaretClickValid = false;
    QString error;
    const QByteArray catalogData = catalogFile.readAll();
    const bool catalogLoaded = useLocalCatalog
        ? controller.loadCatalogTsv(catalogData, &error)
        : controller.loadCatalog(catalogData, &error);
    if (!catalogLoaded) {
        QTextStream(stderr) << "Emoji-cord: " << error << '\n';
        return 1;
    }

    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!demoMode) {
        controller.loadUsage(QDir(dataDirectory).filePath(QStringLiteral("usage.json")));
    }

    if (!demoMode) {
        QString bridgeError;
        if (!contextRouter.registerDBusBridge(&bridgeError)) {
            QTextStream(stderr) << "Emoji-cord: active-application bridge unavailable: "
                                << bridgeError << '\n';
        } else if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/Settings"),
                       &settings, QDBusConnection::ExportScriptableSlots
                           | QDBusConnection::ExportScriptableProperties)) {
            QTextStream(stderr) << "Emoji-cord: settings bridge unavailable: "
                                << QDBusConnection::sessionBus().lastError().message() << '\n';
        }
        QObject::connect(&inputMethod, &WaylandInputMethod::contextActivated,
            &contextRouter, [&contextRouter] { contextRouter.setDirectContextActive(true); });
        QObject::connect(&inputMethod, &WaylandInputMethod::contextDeactivated,
            &contextRouter, [&contextRouter] { contextRouter.setDirectContextActive(false); });
        QObject::connect(&contextRouter, &ContextRouter::activeApplicationChanged,
            &controller, &CompletionController::dismiss);
        if (!parser.isSet(disableFallbackOption)) {
            QString fallbackError;
            if (xwaylandFallback.initialize(&fallbackError)) {
                const auto applyFallbackRoute = [&controller, &xwaylandFallback](
                                                    ContextRouter::Route route) {
                    const bool enabled = route == ContextRouter::Route::Fallback;
                    xwaylandFallback.setEnabled(enabled);
                    controller.setFallbackMode(enabled);
                };
                QObject::connect(&contextRouter, &ContextRouter::routeChanged,
                    &application, applyFallbackRoute);
                QObject::connect(&xwaylandFallback, &XWaylandFallback::steamActiveChanged,
                    &contextRouter, &ContextRouter::setXWaylandSteamActive);
                contextRouter.setXWaylandSteamActive(xwaylandFallback.isSteamActive());
                contextRouter.setFallbackEnabled(true);
                applyFallbackRoute(contextRouter.route());
                QObject::connect(&xwaylandFallback, &XWaylandFallback::characterObserved,
                    &controller, &CompletionController::observeFallbackCharacter);
                QObject::connect(&xwaylandFallback, &XWaylandFallback::backspaceObserved,
                    &controller, &CompletionController::observeFallbackBackspace);
                QObject::connect(&controller, &CompletionController::visibleChanged,
                    &application, [&controller, &xwaylandFallback] {
                        xwaylandFallback.setNavigationActive(controller.isVisible());
                    });
                QObject::connect(&xwaylandFallback, &XWaylandFallback::navigationRequested,
                    &controller, &CompletionController::moveSelection);
                QObject::connect(&xwaylandFallback, &XWaylandFallback::selectionRequested,
                    &controller, [&controller] { controller.select(); });
                QObject::connect(&xwaylandFallback, &XWaylandFallback::dismissalRequested,
                    &controller, &CompletionController::dismiss);
                QObject::connect(&controller, &CompletionController::fallbackCommitRequested,
                    &xwaylandFallback, &XWaylandFallback::replaceShortcode);
                QObject::connect(&xwaylandFallback, &XWaylandFallback::replacementCommitted,
                    &controller, &CompletionController::confirmFallbackCommit);
                QObject::connect(&xwaylandFallback, &XWaylandFallback::routeInvalidated,
                    &controller, &CompletionController::dismiss);
            } else {
                QTextStream(stderr) << "Emoji-cord: XWayland fallback unavailable: "
                                    << fallbackError << '\n';
            }
        }

        auto *native = application.nativeInterface<QNativeInterface::QWaylandApplication>();
        if (!native) {
            QTextStream(stderr) << "Emoji-cord: the input method requires a Wayland session\n";
            return 1;
        }
        if (!inputMethod.connectToCompositor(native->display(), &error)) {
            QTextStream(stderr) << "Emoji-cord: " << error << '\n';
            QTextStream(stderr)
                << "Select Emoji-cord under System Settings > Keyboard > Virtual Keyboard.\n";
            return 1;
        }
    }

    PickerWindow picker(&controller, &settings, &effectCapabilities,
        demoMode ? nullptr : &inputMethod,
        demoMode ? PickerWindow::Mode::Demo : PickerWindow::Mode::InputPanel);
    if (!picker.initialize(&error)) {
        QTextStream(stderr) << "Emoji-cord: " << error << '\n';
        return 1;
    }
    std::unique_ptr<PickerWindow> fallbackPicker;
    if (!demoMode) {
        fallbackPicker = std::make_unique<PickerWindow>(&controller, &settings,
            &effectCapabilities, nullptr, PickerWindow::Mode::Fallback);
        if (!fallbackPicker->initialize(&error)) {
            QTextStream(stderr) << "Emoji-cord: fallback picker unavailable: "
                                << error << '\n';
            fallbackPicker.reset();
        } else if (xwaylandFallback.isAvailable()) {
            QObject::connect(&xwaylandFallback, &XWaylandFallback::targetPositionChanged,
                fallbackPicker.get(), [&controller, window = fallbackPicker.get()](int x, int y) {
                    if (controller.isFallbackMode()) {
                        window->setFallbackPosition(QPoint(x, y));
                    }
                });
            QObject::connect(&xwaylandFallback, &XWaylandFallback::targetPositionInvalidated,
                fallbackPicker.get(), [&controller, window = fallbackPicker.get()] {
                    if (controller.isFallbackMode()) {
                        window->clearFallbackPosition();
                    }
                });
        }
    }
    if (!demoMode && fallbackPicker) {
        const auto updatePositionedDirect = [&controller, &contextRouter, &picker,
                                                &pointerFallbackTimer, &xwaylandFallback,
                                                &lastNoCaretClick, &lastNoCaretClickValid,
                                                window = fallbackPicker.get()] {
            const bool enabled = xwaylandFallback.isAvailable()
                && contextRouter.route() == ContextRouter::Route::Direct
                && contextRouter.activeApplicationId() == QStringLiteral("cudatext");
            picker.setPositionedDirect(enabled);
            window->setPositionedDirect(enabled);
            if (enabled && !controller.isVisible()) {
                QPoint position;
                bool positionValid = lastNoCaretClickValid;
                if (lastNoCaretClickValid) {
                    position = lastNoCaretClick;
                } else {
                    positionValid = xwaylandFallback.currentPointerPosition(&position);
                }
                if (positionValid) {
                    window->setFallbackPosition(position + QPoint(8, 24));
                }
            }
            if (enabled && !pointerFallbackTimer.isActive()) {
                pointerFallbackTimer.start();
            } else if (!enabled) {
                pointerFallbackTimer.stop();
            }
        };
        QObject::connect(&contextRouter, &ContextRouter::activeApplicationChanged,
            &application, [&lastNoCaretClickValid, updatePositionedDirect] {
                lastNoCaretClickValid = false;
                updatePositionedDirect();
            });
        QObject::connect(&contextRouter, &ContextRouter::routeChanged,
            &application, updatePositionedDirect);
        QObject::connect(&pointerFallbackTimer, &QTimer::timeout,
            &application, updatePositionedDirect);
        QObject::connect(&xwaylandFallback, &XWaylandFallback::pointerClicked,
            &application, [&controller, &contextRouter, &lastNoCaretClick,
                              &lastNoCaretClickValid, updatePositionedDirect](const QPoint &position) {
                if (!controller.isVisible()
                    && contextRouter.route() == ContextRouter::Route::Direct
                    && contextRouter.activeApplicationId() == QStringLiteral("cudatext")) {
                    lastNoCaretClick = position;
                    lastNoCaretClickValid = true;
                    updatePositionedDirect();
                }
            });
        QObject::connect(&controller, &CompletionController::visibleChanged,
            &application, [&controller, updatePositionedDirect] {
                if (!controller.isVisible()) {
                    updatePositionedDirect();
                }
            });
        updatePositionedDirect();
    }
    if (demoMode) {
        QObject::connect(&controller, &CompletionController::visibleChanged, &application,
            [&controller] {
                if (!controller.isVisible()) {
                    QGuiApplication::quit();
                }
        });
        controller.preview(QStringLiteral("sk"));
    }

    return application.exec();
}
