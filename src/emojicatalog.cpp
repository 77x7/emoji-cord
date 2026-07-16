// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "emojicatalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>

namespace {
QStringList stringArray(const QJsonValue &value)
{
    QStringList result;
    for (const QJsonValue &item : value.toArray()) {
        const QString text = item.toString().trimmed().toLower();
        if (!text.isEmpty() && !result.contains(text)) {
            result.append(text);
        }
    }
    return result;
}
}

bool EmojiCatalog::loadJson(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (document.isNull() || !document.isArray()) {
        if (error) {
            *error = parseError.error == QJsonParseError::NoError
                ? QStringLiteral("Catalog root must be an array")
                : parseError.errorString();
        }
        return false;
    }

    QVector<EmojiEntry> loaded;
    QSet<QString> aliases;
    const QJsonArray records = document.array();
    loaded.reserve(records.size());

    for (qsizetype index = 0; index < records.size(); ++index) {
        const QJsonObject record = records.at(index).toObject();
        EmojiEntry entry{
            record.value(QStringLiteral("alias")).toString().trimmed().toLower(),
            record.value(QStringLiteral("emoji")).toString(),
            stringArray(record.value(QStringLiteral("aliases"))),
            stringArray(record.value(QStringLiteral("keywords"))),
        };

        if (entry.alias.isEmpty() || entry.emoji.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Catalog record %1 requires alias and emoji").arg(index);
            }
            return false;
        }
        if (aliases.contains(entry.alias)) {
            if (error) {
                *error = QStringLiteral("Duplicate canonical alias: %1").arg(entry.alias);
            }
            return false;
        }

        entry.alternateAliases.removeAll(entry.alias);
        aliases.insert(entry.alias);
        loaded.append(std::move(entry));
    }

    std::sort(loaded.begin(), loaded.end(), [](const EmojiEntry &left, const EmojiEntry &right) {
        return left.alias < right.alias;
    });
    m_entries = std::move(loaded);
    if (error) {
        error->clear();
    }
    return true;
}

bool EmojiCatalog::loadTsv(const QByteArray &tsv, QString *error)
{
    QVector<EmojiEntry> loaded;
    QSet<QString> aliases;
    const QList<QByteArray> lines = tsv.split('\n');
    loaded.reserve(lines.size());

    for (qsizetype index = 0; index < lines.size(); ++index) {
        const QByteArray line = lines.at(index).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const qsizetype tab = line.indexOf('\t');
        if (tab <= 0 || tab + 1 >= line.size()) {
            if (error) {
                *error = QStringLiteral("Invalid TSV catalog record at line %1").arg(index + 1);
            }
            return false;
        }
        EmojiEntry entry{
            QString::fromUtf8(line.first(tab)).trimmed().toLower(),
            QString::fromUtf8(line.sliced(tab + 1)).trimmed(),
            {},
            {},
        };
        if (entry.alias.isEmpty() || entry.emoji.isEmpty() || aliases.contains(entry.alias)) {
            if (error) {
                *error = QStringLiteral("Invalid or duplicate alias at line %1").arg(index + 1);
            }
            return false;
        }
        aliases.insert(entry.alias);
        loaded.append(std::move(entry));
    }

    std::sort(loaded.begin(), loaded.end(), [](const EmojiEntry &left, const EmojiEntry &right) {
        return left.alias < right.alias;
    });
    m_entries = std::move(loaded);
    if (error) {
        error->clear();
    }
    return true;
}

const QVector<EmojiEntry> &EmojiCatalog::entries() const
{
    return m_entries;
}

const EmojiEntry *EmojiCatalog::findExact(QStringView alias) const
{
    const QString normalized = alias.toString().toLower();
    const auto it = std::lower_bound(m_entries.cbegin(), m_entries.cend(), normalized,
        [](const EmojiEntry &entry, const QString &value) {
            return entry.alias < value;
        });
    return it != m_entries.cend() && it->alias == normalized ? &*it : nullptr;
}
