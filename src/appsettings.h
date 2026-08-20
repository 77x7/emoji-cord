// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QJsonValue>
#include <QString>

class AppSettings final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.puzll.EmojiCord.Settings")
    Q_PROPERTY(int visibleSuggestions READ visibleSuggestions NOTIFY visibleSuggestionsChanged)
    Q_PROPERTY(int backgroundOpacity READ backgroundOpacity NOTIFY backgroundOpacityChanged)
    Q_PROPERTY(bool blurEnabled READ blurEnabled NOTIFY blurEnabledChanged)
    Q_PROPERTY(bool contrastEnabled READ contrastEnabled NOTIFY contrastEnabledChanged)
    Q_PROPERTY(bool dynamicWidth READ dynamicWidth NOTIFY dynamicWidthChanged)
    Q_PROPERTY(int pickerWidth READ pickerWidth NOTIFY pickerWidthChanged)
    Q_PROPERTY(int maximumPickerWidth READ maximumPickerWidth NOTIFY maximumPickerWidthChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    static constexpr int defaultVisibleSuggestions = 8;
    static constexpr int automaticBackgroundOpacity = 0;
    static constexpr int defaultPickerWidth = 280;
    static constexpr int defaultMaximumPickerWidth = 480;

    explicit AppSettings(QString path, QObject *parent = nullptr);

    static QString defaultPath();
    int visibleSuggestions() const;
    int backgroundOpacity() const;
    bool blurEnabled() const;
    bool contrastEnabled() const;
    bool dynamicWidth() const;
    int pickerWidth() const;
    int maximumPickerWidth() const;
    QString error() const;
    QString status() const;
    void setStatus(QString status);

public slots:
    Q_SCRIPTABLE bool updateVisibleSuggestions(int value);
    Q_SCRIPTABLE bool updateBackgroundOpacity(int value);
    Q_SCRIPTABLE bool updateBlurEnabled(bool enabled);
    Q_SCRIPTABLE bool updateContrastEnabled(bool enabled);
    Q_SCRIPTABLE bool updateDynamicWidth(bool enabled);
    Q_SCRIPTABLE bool updatePickerWidth(int value);
    Q_SCRIPTABLE bool updateMaximumPickerWidth(int value);
    Q_SCRIPTABLE bool applyVisibleSuggestions(int value);
    Q_SCRIPTABLE bool applyBackgroundOpacity(int value);
    Q_SCRIPTABLE bool applyBlurEnabled(bool enabled);
    Q_SCRIPTABLE bool applyContrastEnabled(bool enabled);
    Q_SCRIPTABLE bool applyDynamicWidth(bool enabled);
    Q_SCRIPTABLE bool applyPickerWidth(int value);
    Q_SCRIPTABLE bool applyMaximumPickerWidth(int value);

signals:
    void visibleSuggestionsChanged(int value);
    void backgroundOpacityChanged(int value);
    void blurEnabledChanged(bool enabled);
    void contrastEnabledChanged(bool enabled);
    void dynamicWidthChanged(bool enabled);
    void pickerWidthChanged(int value);
    void maximumPickerWidthChanged(int value);
    void errorChanged();
    void statusChanged();

private:
    void load();
    bool saveValue(const QString &key, const QJsonValue &value);
    void setError(QString error);

    QString m_path;
    QString m_error;
    QString m_status;
    int m_visibleSuggestions = defaultVisibleSuggestions;
    int m_backgroundOpacity = automaticBackgroundOpacity;
    bool m_blurEnabled = true;
    bool m_contrastEnabled = true;
    bool m_dynamicWidth = false;
    int m_pickerWidth = defaultPickerWidth;
    int m_maximumPickerWidth = defaultMaximumPickerWidth;
};
