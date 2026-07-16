// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QDBusContext>

class ContextRouter final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.puzll.EmojiCord.Context")

public:
    enum class Route {
        None,
        Direct,
        Fallback,
    };
    Q_ENUM(Route)

    explicit ContextRouter(QObject *parent = nullptr);

    bool registerDBusBridge(QString *error = nullptr);
    Route route() const;
    bool isSteamActive() const;
    QString activeApplicationId() const;

    void setDirectContextActive(bool active);
    void setFallbackEnabled(bool enabled);
    void setSessionLocked(bool locked);
    void setXWaylandSteamActive(bool active);

public slots:
    Q_SCRIPTABLE QString debugState() const;
    Q_SCRIPTABLE void activeWindowChanged(const QString &windowId,
        const QString &desktopFileName, const QString &resourceClass,
        const QString &resourceName, bool active);
    void screenLockChanged(bool locked);
    void serviceOwnerChanged(const QString &name, const QString &oldOwner,
        const QString &newOwner);

signals:
    void routeChanged(ContextRouter::Route route);
    void activeApplicationChanged();

private:
    static QString normalizedDesktopFileName(const QString &name);
    bool isKWinCaller() const;
    void clearActiveWindow();
    void updateRoute();

    QString m_windowId;
    QString m_desktopFileName;
    QString m_resourceClass;
    QString m_resourceName;
    QString m_kwinOwner;
    uint m_kwinPid = 0;
    Route m_route = Route::None;
    bool m_windowActive = false;
    bool m_directContextActive = false;
    bool m_fallbackEnabled = false;
    bool m_lockStateKnown = false;
    bool m_sessionLocked = true;
    bool m_xwaylandSteamActive = false;
};
