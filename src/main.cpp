// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "completioncontroller.h"
#include "pickerwindow.h"
#include "waylandinputmethod.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTextStream>
#include <QtGui/qguiapplication_platform.h>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Emoji-cord"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setQuitOnLastWindowClosed(false);

    const bool demoMode = application.arguments().contains(QStringLiteral("--demo-picker"));
    const QString localCatalog = QDir::home().filePath(
        QStringLiteral(".local/share/fcitx5/emojicomplete/emoji.tsv"));
    const bool useLocalCatalog = QFileInfo::exists(localCatalog);
    QFile catalogFile(useLocalCatalog ? localCatalog : QStringLiteral(":/data/emoji-demo.json"));
    if (!catalogFile.open(QIODevice::ReadOnly)) {
        QTextStream(stderr) << "Emoji-cord: cannot open the development catalog\n";
        return 1;
    }

    WaylandInputMethod inputMethod;
    CompletionController controller(demoMode ? nullptr : &inputMethod);
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

    PickerWindow picker(&controller, demoMode ? nullptr : &inputMethod,
        demoMode ? PickerWindow::Mode::Demo : PickerWindow::Mode::InputPanel);
    if (!picker.initialize(&error)) {
        QTextStream(stderr) << "Emoji-cord: " << error << '\n';
        return 1;
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
