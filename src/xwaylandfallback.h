// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

#include <memory>

class XWaylandFallback final : public QObject
{
    Q_OBJECT

public:
    explicit XWaylandFallback(QObject *parent = nullptr);
    ~XWaylandFallback() override;

    bool initialize(QString *error = nullptr);
    void setEnabled(bool enabled);
    void setNavigationActive(bool active);
    bool isAvailable() const;
    bool isSteamActive() const;

public slots:
    void replaceShortcode(int eraseCharacters, const QString &emoji, const QString &alias);

signals:
    void characterObserved(QChar character);
    void backspaceObserved();
    void navigationRequested(int delta);
    void selectionRequested();
    void dismissalRequested();
    void replacementCommitted(const QString &emoji, const QString &alias);
    void steamActiveChanged(bool active);
    void routeInvalidated();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
