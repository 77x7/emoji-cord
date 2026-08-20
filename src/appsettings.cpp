// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appsettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <limits>
#include <utility>

AppSettings::AppSettings(QString path, QObject *parent)
    : QObject(parent)
    , m_path(std::move(path))
{
    load();
}

QString AppSettings::defaultPath()
{
    const QString configRoot = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    return QDir(configRoot).filePath(QStringLiteral("emoji-cord/settings.json"));
}

int AppSettings::visibleSuggestions() const
{
    return m_visibleSuggestions;
}

int AppSettings::backgroundOpacity() const
{
    return m_backgroundOpacity;
}

bool AppSettings::blurEnabled() const
{
    return m_blurEnabled;
}

bool AppSettings::contrastEnabled() const
{
    return m_contrastEnabled;
}

bool AppSettings::dynamicWidth() const
{
    return m_dynamicWidth;
}

int AppSettings::pickerWidth() const
{
    return m_pickerWidth;
}

int AppSettings::maximumPickerWidth() const
{
    return m_maximumPickerWidth;
}

QString AppSettings::error() const
{
    return m_error;
}

QString AppSettings::status() const
{
    return m_status;
}

void AppSettings::setStatus(QString status)
{
    if (m_status == status) {
        return;
    }
    m_status = std::move(status);
    emit statusChanged();
}

bool AppSettings::updateVisibleSuggestions(int value)
{
    setStatus({});
    if (value < 1) {
        setError(QStringLiteral("Suggestions shown at once must be a positive integer."));
        return false;
    }
    if (value == m_visibleSuggestions) {
        if (!m_error.isEmpty() && !saveValue(QStringLiteral("visibleSuggestions"), value)) {
            return false;
        }
        setError({});
        return true;
    }
    if (!saveValue(QStringLiteral("visibleSuggestions"), value)) {
        return false;
    }

    m_visibleSuggestions = value;
    setError({});
    emit visibleSuggestionsChanged(value);
    return true;
}

bool AppSettings::updateBackgroundOpacity(int value)
{
    setStatus({});
    if (value != automaticBackgroundOpacity && (value < 20 || value > 100)) {
        setError(QStringLiteral("Background opacity must be automatic or between 20 and 100 percent."));
        return false;
    }
    if (value == m_backgroundOpacity) {
        if (!m_error.isEmpty() && !saveValue(QStringLiteral("backgroundOpacity"), value)) {
            return false;
        }
        setError({});
        return true;
    }
    if (!saveValue(QStringLiteral("backgroundOpacity"), value)) {
        return false;
    }
    m_backgroundOpacity = value;
    setError({});
    emit backgroundOpacityChanged(value);
    return true;
}

bool AppSettings::updateBlurEnabled(bool enabled)
{
    setStatus({});
    if (enabled == m_blurEnabled) {
        if (!m_error.isEmpty() && !saveValue(QStringLiteral("blurEnabled"), enabled)) {
            return false;
        }
        setError({});
        return true;
    }
    if (!saveValue(QStringLiteral("blurEnabled"), enabled)) {
        return false;
    }
    m_blurEnabled = enabled;
    setError({});
    emit blurEnabledChanged(enabled);
    return true;
}

bool AppSettings::updateContrastEnabled(bool enabled)
{
    setStatus({});
    if (enabled == m_contrastEnabled) {
        if (!m_error.isEmpty() && !saveValue(QStringLiteral("contrastEnabled"), enabled)) {
            return false;
        }
        setError({});
        return true;
    }
    if (!saveValue(QStringLiteral("contrastEnabled"), enabled)) {
        return false;
    }
    m_contrastEnabled = enabled;
    setError({});
    emit contrastEnabledChanged(enabled);
    return true;
}

bool AppSettings::updateDynamicWidth(bool enabled)
{
    setStatus({});
    if (enabled == m_dynamicWidth) {
        if (!m_error.isEmpty() && !saveValue(QStringLiteral("dynamicWidth"), enabled)) {
            return false;
        }
        setError({});
        return true;
    }
    if (!saveValue(QStringLiteral("dynamicWidth"), enabled)) {
        return false;
    }
    m_dynamicWidth = enabled;
    setError({});
    emit dynamicWidthChanged(enabled);
    return true;
}

bool AppSettings::updatePickerWidth(int value)
{
    setStatus({});
    if (value < 220 || value > 2000) {
        setError(QStringLiteral("Picker width must be between 220 and 2000 pixels."));
        return false;
    }
    if (value == m_pickerWidth) {
        if (!m_error.isEmpty() && !saveValue(QStringLiteral("pickerWidth"), value)) {
            return false;
        }
        setError({});
        return true;
    }
    if (!saveValue(QStringLiteral("pickerWidth"), value)) {
        return false;
    }
    m_pickerWidth = value;
    setError({});
    emit pickerWidthChanged(value);
    return true;
}

bool AppSettings::updateMaximumPickerWidth(int value)
{
    setStatus({});
    if (value < 220 || value > 2000) {
        setError(QStringLiteral("Maximum picker width must be between 220 and 2000 pixels."));
        return false;
    }
    if (value == m_maximumPickerWidth) {
        if (!m_error.isEmpty() && !saveValue(QStringLiteral("maximumPickerWidth"), value)) {
            return false;
        }
        setError({});
        return true;
    }
    if (!saveValue(QStringLiteral("maximumPickerWidth"), value)) {
        return false;
    }
    m_maximumPickerWidth = value;
    setError({});
    emit maximumPickerWidthChanged(value);
    return true;
}

bool AppSettings::applyVisibleSuggestions(int value)
{
    if (value < 1) {
        return false;
    }
    if (m_visibleSuggestions != value) {
        m_visibleSuggestions = value;
        emit visibleSuggestionsChanged(value);
    }
    return true;
}

bool AppSettings::applyBackgroundOpacity(int value)
{
    if (value != automaticBackgroundOpacity && (value < 20 || value > 100)) {
        return false;
    }
    if (m_backgroundOpacity != value) {
        m_backgroundOpacity = value;
        emit backgroundOpacityChanged(value);
    }
    return true;
}

bool AppSettings::applyBlurEnabled(bool enabled)
{
    if (m_blurEnabled != enabled) {
        m_blurEnabled = enabled;
        emit blurEnabledChanged(enabled);
    }
    return true;
}

bool AppSettings::applyContrastEnabled(bool enabled)
{
    if (m_contrastEnabled != enabled) {
        m_contrastEnabled = enabled;
        emit contrastEnabledChanged(enabled);
    }
    return true;
}

bool AppSettings::applyDynamicWidth(bool enabled)
{
    if (m_dynamicWidth != enabled) {
        m_dynamicWidth = enabled;
        emit dynamicWidthChanged(enabled);
    }
    return true;
}

bool AppSettings::applyPickerWidth(int value)
{
    if (value < 220 || value > 2000) {
        return false;
    }
    if (m_pickerWidth != value) {
        m_pickerWidth = value;
        emit pickerWidthChanged(value);
    }
    return true;
}

bool AppSettings::applyMaximumPickerWidth(int value)
{
    if (value < 220 || value > 2000) {
        return false;
    }
    if (m_maximumPickerWidth != value) {
        m_maximumPickerWidth = value;
        emit maximumPickerWidthChanged(value);
    }
    return true;
}

void AppSettings::load()
{
    QFile file(m_path);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Could not read settings: %1").arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject object = document.object();
    QJsonValue visibleValue = object.value(QStringLiteral("visibleSuggestions"));
    if (visibleValue.isUndefined()) {
        visibleValue = object.value(QStringLiteral("maxSuggestions"));
    }
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || !visibleValue.isDouble() || visibleValue.toDouble() < 1
        || visibleValue.toDouble() > double(std::numeric_limits<int>::max())
        || visibleValue.toInt() != visibleValue.toDouble()) {
        setError(QStringLiteral("Settings file contains an invalid visible suggestion count."));
        return;
    }

    const QJsonValue opacityValue = object.value(QStringLiteral("backgroundOpacity"));
    const int opacity = opacityValue.isUndefined()
        ? automaticBackgroundOpacity : opacityValue.toInt(-1);
    if ((!opacityValue.isUndefined() && (!opacityValue.isDouble()
            || opacityValue.toDouble() != opacity))
        || (opacity != automaticBackgroundOpacity && (opacity < 20 || opacity > 100))) {
        setError(QStringLiteral("Settings file contains an invalid background opacity."));
        return;
    }
    const QJsonValue blurValue = object.value(QStringLiteral("blurEnabled"));
    const QJsonValue contrastValue = object.value(QStringLiteral("contrastEnabled"));
    const QJsonValue dynamicWidthValue = object.value(QStringLiteral("dynamicWidth"));
    const QJsonValue pickerWidthValue = object.value(QStringLiteral("pickerWidth"));
    const QJsonValue maximumPickerWidthValue = object.value(QStringLiteral("maximumPickerWidth"));
    if ((!blurValue.isUndefined() && !blurValue.isBool())
        || (!contrastValue.isUndefined() && !contrastValue.isBool())
        || (!dynamicWidthValue.isUndefined() && !dynamicWidthValue.isBool())) {
        setError(QStringLiteral("Settings file contains an invalid window effect option."));
        return;
    }
    const int pickerWidth = pickerWidthValue.isUndefined()
        ? defaultPickerWidth : pickerWidthValue.toInt(-1);
    const int maximumPickerWidth = maximumPickerWidthValue.isUndefined()
        ? defaultMaximumPickerWidth : maximumPickerWidthValue.toInt(-1);
    if ((!pickerWidthValue.isUndefined() && (!pickerWidthValue.isDouble()
            || pickerWidthValue.toDouble() != pickerWidth))
        || (!maximumPickerWidthValue.isUndefined() && (!maximumPickerWidthValue.isDouble()
            || maximumPickerWidthValue.toDouble() != maximumPickerWidth))
        || pickerWidth < 220 || pickerWidth > 2000
        || maximumPickerWidth < 220 || maximumPickerWidth > 2000) {
        setError(QStringLiteral("Settings file contains an invalid picker width."));
        return;
    }

    m_visibleSuggestions = visibleValue.toInt();
    m_backgroundOpacity = opacity;
    m_blurEnabled = blurValue.isUndefined() ? true : blurValue.toBool();
    m_contrastEnabled = contrastValue.isUndefined() ? true : contrastValue.toBool();
    m_dynamicWidth = dynamicWidthValue.isUndefined() ? false : dynamicWidthValue.toBool();
    m_pickerWidth = pickerWidth;
    m_maximumPickerWidth = maximumPickerWidth;
}

bool AppSettings::saveValue(const QString &key, const QJsonValue &value)
{
    const QFileInfo fileInfo(m_path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        setError(QStringLiteral("Could not create the settings directory."));
        return false;
    }

    QJsonObject object;
    QFile existing(m_path);
    if (existing.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(existing.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            object = document.object();
        }
    }
    if (!m_error.isEmpty()) {
        object = {};
    }
    if (!object.contains(QStringLiteral("visibleSuggestions"))) {
        object.insert(QStringLiteral("visibleSuggestions"), m_visibleSuggestions);
    }
    if (!object.contains(QStringLiteral("backgroundOpacity"))) {
        object.insert(QStringLiteral("backgroundOpacity"), m_backgroundOpacity);
    }
    if (!object.contains(QStringLiteral("blurEnabled"))) {
        object.insert(QStringLiteral("blurEnabled"), m_blurEnabled);
    }
    if (!object.contains(QStringLiteral("contrastEnabled"))) {
        object.insert(QStringLiteral("contrastEnabled"), m_contrastEnabled);
    }
    if (!object.contains(QStringLiteral("dynamicWidth"))) {
        object.insert(QStringLiteral("dynamicWidth"), m_dynamicWidth);
    }
    if (!object.contains(QStringLiteral("pickerWidth"))) {
        object.insert(QStringLiteral("pickerWidth"), m_pickerWidth);
    }
    if (!object.contains(QStringLiteral("maximumPickerWidth"))) {
        object.insert(QStringLiteral("maximumPickerWidth"), m_maximumPickerWidth);
    }
    object.insert(key, value);
    if (key == QStringLiteral("visibleSuggestions")) {
        object.remove(QStringLiteral("maxSuggestions"));
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Could not save settings: %1").arg(file.errorString()));
        return false;
    }
    const QJsonDocument document(object);
    const QByteArray contents = document.toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size() || !file.commit()) {
        setError(QStringLiteral("Could not save settings: %1").arg(file.errorString()));
        return false;
    }
    return true;
}

void AppSettings::setError(QString error)
{
    if (m_error == error) {
        return;
    }
    m_error = std::move(error);
    emit errorChanged();
}
