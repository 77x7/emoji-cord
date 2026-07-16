// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "usagestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>

bool UsageStore::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.exists()) {
        clear();
        if (error) {
            error->clear();
        }
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        if (error) {
            *error = parseError.error == QJsonParseError::NoError
                ? QStringLiteral("Usage root must be an object")
                : parseError.errorString();
        }
        return false;
    }

    QHash<QString, EmojiUsage> loaded;
    quint64 sequence = 0;
    const QJsonObject root = document.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QJsonObject value = it.value().toObject();
        const qint64 count = value.value(QStringLiteral("count")).toInteger(-1);
        const qint64 lastUsed = value.value(QStringLiteral("lastUsed")).toInteger(-1);
        if (count < 0 || lastUsed < 0) {
            if (error) {
                *error = QStringLiteral("Invalid usage record: %1").arg(it.key());
            }
            return false;
        }
        loaded.insert(it.key().toLower(), {quint64(count), quint64(lastUsed)});
        sequence = std::max(sequence, quint64(lastUsed));
    }

    m_usage = std::move(loaded);
    m_sequence = sequence;
    if (error) {
        error->clear();
    }
    return true;
}

bool UsageStore::save(const QString &path, QString *error) const
{
    const QFileInfo info(path);
    if (!info.dir().mkpath(QStringLiteral("."))) {
        if (error) {
            *error = QStringLiteral("Cannot create usage directory: %1").arg(info.dir().path());
        }
        return false;
    }

    QJsonObject root;
    for (auto it = m_usage.cbegin(); it != m_usage.cend(); ++it) {
        QJsonObject value;
        value.insert(QStringLiteral("count"), qint64(it->count));
        value.insert(QStringLiteral("lastUsed"), qint64(it->lastUsed));
        root.insert(it.key(), value);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

EmojiUsage UsageStore::usage(QStringView alias) const
{
    return m_usage.value(alias.toString().toLower());
}

void UsageStore::record(QStringView alias)
{
    EmojiUsage &value = m_usage[alias.toString().toLower()];
    ++value.count;
    value.lastUsed = ++m_sequence;
}

void UsageStore::clear()
{
    m_usage.clear();
    m_sequence = 0;
}
