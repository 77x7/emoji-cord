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
        if (!m_error.isEmpty() && !save(value)) {
            return false;
        }
        setError({});
        return true;
    }
    if (!save(value)) {
        return false;
    }

    m_visibleSuggestions = value;
    setError({});
    emit visibleSuggestionsChanged(value);
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
    QJsonValue value = object.value(QStringLiteral("visibleSuggestions"));
    if (value.isUndefined()) {
        value = object.value(QStringLiteral("maxSuggestions"));
    }
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || !value.isDouble() || value.toDouble() < 1
        || value.toDouble() > double(std::numeric_limits<int>::max())
        || value.toInt() != value.toDouble()) {
        setError(QStringLiteral("Settings file contains an invalid visible suggestion count."));
        return;
    }
    m_visibleSuggestions = value.toInt();
}

bool AppSettings::save(int value)
{
    const QFileInfo fileInfo(m_path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        setError(QStringLiteral("Could not create the settings directory."));
        return false;
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Could not save settings: %1").arg(file.errorString()));
        return false;
    }
    const QJsonDocument document(QJsonObject{{QStringLiteral("visibleSuggestions"), value}});
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
