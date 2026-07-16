// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

struct EmojiEntry {
    QString alias;
    QString emoji;
    QStringList alternateAliases;
    QStringList keywords;

    bool operator==(const EmojiEntry &) const = default;
};

class EmojiCatalog
{
public:
    bool loadJson(const QByteArray &json, QString *error = nullptr);
    bool loadTsv(const QByteArray &tsv, QString *error = nullptr);

    const QVector<EmojiEntry> &entries() const;
    const EmojiEntry *findExact(QStringView alias) const;

private:
    QVector<EmojiEntry> m_entries;
};
