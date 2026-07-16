// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "contextrouter.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QFileInfo>

ContextRouter::ContextRouter(QObject *parent)
    : QObject(parent)
{
}

bool ContextRouter::registerDBusBridge(QString *error)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("io.github.puzll.EmojiCord"))) {
        if (error) {
            *error = bus.lastError().message();
        }
        return false;
    }
    if (!bus.registerObject(QStringLiteral("/Context"), this,
            QDBusConnection::ExportScriptableSlots)) {
        if (error) {
            *error = bus.lastError().message();
        }
        bus.unregisterService(QStringLiteral("io.github.puzll.EmojiCord"));
        return false;
    }

    bus.connect(QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("/ScreenSaver"), QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("ActiveChanged"), this, SLOT(screenLockChanged(bool)));
    bus.connect(QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"), this,
        SLOT(serviceOwnerChanged(QString,QString,QString)));
    const QDBusReply<QString> kwinOwner = bus.interface()->serviceOwner(
        QStringLiteral("org.kde.KWin"));
    if (kwinOwner.isValid()) {
        m_kwinOwner = kwinOwner.value();
    }
    const QDBusReply<uint> kwinPid = bus.interface()->servicePid(
        QStringLiteral("org.kde.KWin"));
    if (kwinPid.isValid()) {
        m_kwinPid = kwinPid.value();
    }
    QDBusInterface screenSaver(QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("/ScreenSaver"), QStringLiteral("org.freedesktop.ScreenSaver"), bus);
    const QDBusReply<bool> locked = screenSaver.call(QStringLiteral("GetActive"));
    if (locked.isValid()) {
        setSessionLocked(locked.value());
    }
    return true;
}

ContextRouter::Route ContextRouter::route() const
{
    return m_route;
}

bool ContextRouter::isSteamActive() const
{
    if (!m_windowActive || m_windowId.isEmpty()) {
        return false;
    }

    const QString desktopFileName = normalizedDesktopFileName(m_desktopFileName);
    if (desktopFileName == QStringLiteral("steam")
        || desktopFileName == QStringLiteral("com.valvesoftware.steam")) {
        return true;
    }

    return m_resourceClass.compare(QStringLiteral("steam"), Qt::CaseInsensitive) == 0
        || m_resourceName.compare(QStringLiteral("steam"), Qt::CaseInsensitive) == 0;
}

QString ContextRouter::activeApplicationId() const
{
    const QString desktopFileName = normalizedDesktopFileName(m_desktopFileName);
    if (!desktopFileName.isEmpty()) {
        return desktopFileName;
    }
    if (!m_resourceClass.isEmpty()) {
        return m_resourceClass.toCaseFolded();
    }
    return m_resourceName.toCaseFolded();
}

void ContextRouter::setDirectContextActive(bool active)
{
    if (m_directContextActive == active) {
        return;
    }
    m_directContextActive = active;
    updateRoute();
}

void ContextRouter::setFallbackEnabled(bool enabled)
{
    if (m_fallbackEnabled == enabled) {
        return;
    }
    m_fallbackEnabled = enabled;
    updateRoute();
}

void ContextRouter::setSessionLocked(bool locked)
{
    if (m_lockStateKnown && m_sessionLocked == locked) {
        return;
    }
    m_lockStateKnown = true;
    m_sessionLocked = locked;
    updateRoute();
}

void ContextRouter::setXWaylandSteamActive(bool active)
{
    if (m_xwaylandSteamActive == active) {
        return;
    }
    m_xwaylandSteamActive = active;
    updateRoute();
}

QString ContextRouter::debugState() const
{
    const char *routeName = m_route == Route::Direct ? "direct"
        : m_route == Route::Fallback ? "fallback" : "none";
    return QStringLiteral("route=%1 active=%2 app=%3 class=%4 name=%5 direct=%6 fallback=%7 lockKnown=%8 locked=%9")
        .arg(QString::fromLatin1(routeName), m_windowActive ? QStringLiteral("true") : QStringLiteral("false"),
            activeApplicationId(), m_resourceClass, m_resourceName,
            m_directContextActive ? QStringLiteral("true") : QStringLiteral("false"),
            m_fallbackEnabled ? QStringLiteral("true") : QStringLiteral("false"),
            m_lockStateKnown ? QStringLiteral("true") : QStringLiteral("false"),
            m_sessionLocked ? QStringLiteral("true") : QStringLiteral("false"));
}

void ContextRouter::activeWindowChanged(const QString &windowId,
    const QString &desktopFileName, const QString &resourceClass,
    const QString &resourceName, bool active)
{
    if (calledFromDBus() && !isKWinCaller()) {
        return;
    }

    const bool changed = m_windowId != windowId
        || m_desktopFileName != desktopFileName
        || m_resourceClass != resourceClass
        || m_resourceName != resourceName
        || m_windowActive != active;
    m_windowId = windowId;
    m_desktopFileName = desktopFileName;
    m_resourceClass = resourceClass;
    m_resourceName = resourceName;
    m_windowActive = active;
    if (changed) {
        emit activeApplicationChanged();
    }
    updateRoute();
}

void ContextRouter::screenLockChanged(bool locked)
{
    setSessionLocked(locked);
}

void ContextRouter::serviceOwnerChanged(const QString &name, const QString &oldOwner,
    const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    if (name == QStringLiteral("org.kde.KWin")) {
        m_kwinOwner = newOwner;
        const QDBusReply<uint> kwinPid = QDBusConnection::sessionBus().interface()->servicePid(name);
        m_kwinPid = kwinPid.isValid() ? kwinPid.value() : 0;
        clearActiveWindow();
    } else if (name == QStringLiteral("org.freedesktop.ScreenSaver")) {
        m_lockStateKnown = false;
        m_sessionLocked = true;
        updateRoute();
        if (!newOwner.isEmpty()) {
            QDBusInterface screenSaver(name, QStringLiteral("/ScreenSaver"), name,
                QDBusConnection::sessionBus());
            const QDBusReply<bool> locked = screenSaver.call(QStringLiteral("GetActive"));
            if (locked.isValid()) {
                setSessionLocked(locked.value());
            }
        }
    }
}

QString ContextRouter::normalizedDesktopFileName(const QString &name)
{
    QString normalized = QFileInfo(name).fileName().toCaseFolded();
    if (normalized.endsWith(QStringLiteral(".desktop"))) {
        normalized.chop(8);
    }
    return normalized;
}

bool ContextRouter::isKWinCaller() const
{
    if (!m_kwinOwner.isEmpty() && m_kwinOwner == message().service()) {
        return true;
    }
    const QDBusReply<uint> senderPid = QDBusConnection::sessionBus().interface()->servicePid(
        message().service());
    return m_kwinPid != 0 && senderPid.isValid() && senderPid.value() == m_kwinPid;
}

void ContextRouter::clearActiveWindow()
{
    m_windowId.clear();
    m_desktopFileName.clear();
    m_resourceClass.clear();
    m_resourceName.clear();
    m_windowActive = false;
    emit activeApplicationChanged();
    updateRoute();
}

void ContextRouter::updateRoute()
{
    Route next = Route::None;
    if (m_directContextActive) {
        next = Route::Direct;
    } else if (m_fallbackEnabled && m_lockStateKnown && !m_sessionLocked
        && (isSteamActive() || m_xwaylandSteamActive)) {
        next = Route::Fallback;
    }

    if (next == m_route) {
        return;
    }
    m_route = next;
    emit routeChanged(m_route);
}
