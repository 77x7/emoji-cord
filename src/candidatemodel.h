// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "emojimatcher.h"

#include <QAbstractListModel>

class CandidateModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)

public:
    enum Role {
        EmojiRole = Qt::UserRole + 1,
        AliasRole,
        MatchTierRole,
        SelectedRole,
    };
    Q_ENUM(Role)

    explicit CandidateModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMatches(const QVector<EmojiMatch> &matches);
    void clear();
    int selectedIndex() const;
    void setSelectedIndex(int index);
    const EmojiEntry *selectedEntry() const;
    const EmojiEntry *entryAt(int index) const;

signals:
    void selectedIndexChanged();

private:
    struct Row {
        const EmojiEntry *entry = nullptr;
        MatchTier tier = MatchTier::Fuzzy;
    };

    QVector<Row> m_rows;
    int m_selectedIndex = -1;
};
