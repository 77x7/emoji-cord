// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

class AppSettings final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.puzll.EmojiCord.Settings")
    Q_PROPERTY(int visibleSuggestions READ visibleSuggestions NOTIFY visibleSuggestionsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    static constexpr int defaultVisibleSuggestions = 8;

    explicit AppSettings(QString path, QObject *parent = nullptr);

    static QString defaultPath();
    int visibleSuggestions() const;
    QString error() const;
    QString status() const;
    void setStatus(QString status);

public slots:
    Q_SCRIPTABLE bool updateVisibleSuggestions(int value);

signals:
    void visibleSuggestionsChanged(int value);
    void errorChanged();
    void statusChanged();

private:
    void load();
    bool save(int value);
    void setError(QString error);

    QString m_path;
    QString m_error;
    QString m_status;
    int m_visibleSuggestions = defaultVisibleSuggestions;
};
