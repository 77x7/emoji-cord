// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "emojimatcher.h"

#include <algorithm>
#include <limits>

namespace {
constexpr qsizetype noDistance = std::numeric_limits<qsizetype>::max();

QStringList searchableAliases(const EmojiEntry &entry)
{
    QStringList aliases{entry.alias};
    aliases.append(entry.alternateAliases);
    return aliases;
}

bool betterCandidate(const EmojiMatch &left, const EmojiMatch &right)
{
    if (left.tier != right.tier) {
        return left.tier < right.tier;
    }
    if (left.usage.count != right.usage.count) {
        return left.usage.count > right.usage.count;
    }
    if (left.usage.lastUsed != right.usage.lastUsed) {
        return left.usage.lastUsed > right.usage.lastUsed;
    }
    if (left.editDistance != right.editDistance) {
        return left.editDistance < right.editDistance;
    }
    if (left.matchedText.size() != right.matchedText.size()) {
        return left.matchedText.size() < right.matchedText.size();
    }
    return left.entry->alias < right.entry->alias;
}
}

QVector<EmojiMatch> EmojiMatcher::match(const EmojiCatalog &catalog, QStringView query,
    const UsageStore &usage, qsizetype limit)
{
    const QString normalized = query.trimmed().toString().toLower();
    if (normalized.isEmpty() || limit <= 0) {
        return {};
    }

    QVector<EmojiMatch> matches;
    matches.reserve(std::min(limit, catalog.entries().size()));

    for (const EmojiEntry &entry : catalog.entries()) {
        EmojiMatch candidate{&entry, MatchTier::Fuzzy, {}, noDistance, usage.usage(entry.alias)};
        bool found = false;
        const QStringList aliases = searchableAliases(entry);

        for (const QString &alias : aliases) {
            MatchTier tier;
            if (alias == normalized) {
                tier = MatchTier::Exact;
            } else if (alias.startsWith(normalized)) {
                tier = MatchTier::Prefix;
            } else if (alias.contains(normalized)) {
                tier = MatchTier::Substring;
            } else {
                continue;
            }

            EmojiMatch current{&entry, tier, alias, 0, candidate.usage};
            if (!found || betterCandidate(current, candidate)) {
                candidate = std::move(current);
                found = true;
            }
        }

        if (!found) {
            for (const QString &keyword : entry.keywords) {
                if (keyword.contains(normalized)) {
                    candidate.tier = MatchTier::Substring;
                    candidate.matchedText = keyword;
                    candidate.editDistance = 0;
                    found = true;
                    break;
                }
            }
        }

        if (!found && normalized.size() >= 3) {
            const qsizetype maximumDistance = normalized.size() <= 5 ? 1 : 2;
            for (const QString &alias : aliases) {
                const qsizetype distance = damerauLevenshtein(normalized, alias, maximumDistance);
                if (distance <= maximumDistance
                    && (!found || distance < candidate.editDistance
                        || (distance == candidate.editDistance && alias.size() < candidate.matchedText.size()))) {
                    candidate.tier = MatchTier::Fuzzy;
                    candidate.matchedText = alias;
                    candidate.editDistance = distance;
                    found = true;
                }
            }
        }

        if (found) {
            matches.append(std::move(candidate));
        }
    }

    std::sort(matches.begin(), matches.end(), betterCandidate);
    if (matches.size() > limit) {
        matches.resize(limit);
    }
    return matches;
}

qsizetype EmojiMatcher::damerauLevenshtein(QStringView left, QStringView right,
    qsizetype maximumDistance)
{
    if (qAbs(left.size() - right.size()) > maximumDistance) {
        return noDistance;
    }

    QVector<qsizetype> previousPrevious(right.size() + 1);
    QVector<qsizetype> previous(right.size() + 1);
    QVector<qsizetype> current(right.size() + 1);
    for (qsizetype column = 0; column <= right.size(); ++column) {
        previous[column] = column;
    }

    for (qsizetype row = 1; row <= left.size(); ++row) {
        current[0] = row;
        qsizetype rowMinimum = current[0];
        for (qsizetype column = 1; column <= right.size(); ++column) {
            const qsizetype substitutionCost = left.at(row - 1) == right.at(column - 1) ? 0 : 1;
            current[column] = std::min({
                previous[column] + 1,
                current[column - 1] + 1,
                previous[column - 1] + substitutionCost,
            });

            if (row > 1 && column > 1 && left.at(row - 1) == right.at(column - 2)
                && left.at(row - 2) == right.at(column - 1)) {
                current[column] = std::min(current[column], previousPrevious[column - 2] + 1);
            }
            rowMinimum = std::min(rowMinimum, current[column]);
        }
        if (rowMinimum > maximumDistance) {
            return noDistance;
        }
        previousPrevious.swap(previous);
        previous.swap(current);
    }
    return previous.at(right.size());
}
