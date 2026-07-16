// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QString>

struct EmojiUsage {
    quint64 count = 0;
    quint64 lastUsed = 0;

    bool operator==(const EmojiUsage &) const = default;
};

class UsageStore
{
public:
    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;

    EmojiUsage usage(QStringView alias) const;
    void record(QStringView alias);
    void clear();

private:
    QHash<QString, EmojiUsage> m_usage;
    quint64 m_sequence = 0;
};
