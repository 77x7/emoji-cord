// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "emojicatalog.h"
#include "usagestore.h"

#include <QVector>

enum class MatchTier {
    Exact,
    Prefix,
    Substring,
    Fuzzy,
};

struct EmojiMatch {
    const EmojiEntry *entry = nullptr;
    MatchTier tier = MatchTier::Fuzzy;
    QString matchedText;
    qsizetype editDistance = 0;
    EmojiUsage usage;
};

class EmojiMatcher
{
public:
    static QVector<EmojiMatch> match(const EmojiCatalog &catalog, QStringView query,
        const UsageStore &usage, qsizetype limit = 80);

private:
    static qsizetype damerauLevenshtein(QStringView left, QStringView right,
        qsizetype maximumDistance);
};
