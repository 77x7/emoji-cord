// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

class QueryState
{
public:
    enum class Change {
        Ignored,
        Armed,
        Updated,
        ExactRequested,
        Cancelled,
    };

    Change input(QChar character);
    Change backspace();
    Change cancel();

    bool isArmed() const;
    QStringView query() const;
    qsizetype shortcodeLength(bool includesClosingColon = false) const;

private:
    static bool isAliasCharacter(QChar character);

    bool m_armed = false;
    QString m_query;
};
