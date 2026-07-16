// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "querystate.h"

namespace {
constexpr qsizetype maximumQueryLength = 64;
}

QueryState::Change QueryState::input(QChar character)
{
    if (character == u':') {
        if (!m_armed) {
            m_armed = true;
            m_query.clear();
            return Change::Armed;
        }
        if (!m_query.isEmpty()) {
            return Change::ExactRequested;
        }
        return cancel();
    }

    if (!m_armed) {
        return Change::Ignored;
    }
    if (!isAliasCharacter(character) || m_query.size() >= maximumQueryLength) {
        return cancel();
    }

    m_query.append(character.toLower());
    return Change::Updated;
}

QueryState::Change QueryState::backspace()
{
    if (!m_armed) {
        return Change::Ignored;
    }
    if (m_query.isEmpty()) {
        return cancel();
    }

    m_query.chop(1);
    return Change::Updated;
}

QueryState::Change QueryState::cancel()
{
    if (!m_armed) {
        return Change::Ignored;
    }
    m_armed = false;
    m_query.clear();
    return Change::Cancelled;
}

bool QueryState::isArmed() const
{
    return m_armed;
}

QStringView QueryState::query() const
{
    return m_query;
}

qsizetype QueryState::shortcodeLength(bool includesClosingColon) const
{
    return m_armed ? 1 + m_query.size() + (includesClosingColon ? 1 : 0) : 0;
}

bool QueryState::isAliasCharacter(QChar character)
{
    return character.isLetterOrNumber() || character == u'_' || character == u'+'
        || character == u'-';
}
