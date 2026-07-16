// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "candidatemodel.h"

#include <algorithm>

CandidateModel::CandidateModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CandidateModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant CandidateModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case EmojiRole:
        return row.entry->emoji;
    case AliasRole:
        return QStringLiteral(":%1:").arg(row.entry->alias);
    case MatchTierRole:
        return int(row.tier);
    case SelectedRole:
        return index.row() == m_selectedIndex;
    default:
        return {};
    }
}

QHash<int, QByteArray> CandidateModel::roleNames() const
{
    return {
        {EmojiRole, "emoji"},
        {AliasRole, "alias"},
        {MatchTierRole, "matchTier"},
        {SelectedRole, "selected"},
    };
}

void CandidateModel::setMatches(const QVector<EmojiMatch> &matches)
{
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(matches.size());
    for (qsizetype index = 0; index < matches.size(); ++index) {
        m_rows.append({matches.at(index).entry, matches.at(index).tier});
    }
    m_selectedIndex = m_rows.isEmpty() ? -1 : 0;
    endResetModel();
    emit selectedIndexChanged();
}

void CandidateModel::clear()
{
    if (m_rows.isEmpty()) {
        return;
    }
    beginResetModel();
    m_rows.clear();
    m_selectedIndex = -1;
    endResetModel();
    emit selectedIndexChanged();
}

int CandidateModel::selectedIndex() const
{
    return m_selectedIndex;
}

void CandidateModel::setSelectedIndex(int index)
{
    if (m_rows.isEmpty()) {
        index = -1;
    } else {
        index = std::clamp(index, 0, int(m_rows.size() - 1));
    }
    if (m_selectedIndex == index) {
        return;
    }
    const int previous = m_selectedIndex;
    m_selectedIndex = index;
    if (previous >= 0) {
        emit dataChanged(this->index(previous), this->index(previous), {SelectedRole});
    }
    if (index >= 0) {
        emit dataChanged(this->index(index), this->index(index), {SelectedRole});
    }
    emit selectedIndexChanged();
}

const EmojiEntry *CandidateModel::selectedEntry() const
{
    return entryAt(m_selectedIndex);
}

const EmojiEntry *CandidateModel::entryAt(int index) const
{
    return index >= 0 && index < m_rows.size() ? m_rows.at(index).entry : nullptr;
}
