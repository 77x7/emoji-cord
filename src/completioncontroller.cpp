// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "completioncontroller.h"

#include <xkbcommon/xkbcommon-keysyms.h>

#include <utility>

CompletionController::CompletionController(WaylandInputMethod *inputMethod, QObject *parent)
    : QObject(parent)
    , m_inputMethod(inputMethod)
    , m_candidates(this)
{
    if (m_inputMethod) {
        m_inputMethod->setKeyHandler([this](const WaylandInputMethod::KeyEvent &event) {
            return handleKey(event);
        });
        connect(m_inputMethod, &WaylandInputMethod::contextDeactivated, this, [this] {
            reset();
            m_consumedKeys.clear();
        });
        connect(m_inputMethod, &WaylandInputMethod::contextActivated, this,
            &CompletionController::reset);
        connect(m_inputMethod, &WaylandInputMethod::resetRequested,
            this, &CompletionController::reset);
        connect(m_inputMethod, &WaylandInputMethod::sensitiveChanged, this, [this](bool sensitive) {
            if (sensitive) {
                reset();
            }
        });
    }
}

CompletionController::~CompletionController()
{
    if (m_inputMethod) {
        m_inputMethod->setKeyHandler({});
    }
}

bool CompletionController::loadCatalog(const QByteArray &json, QString *error)
{
    const bool loaded = m_catalog.loadJson(json, error);
    if (loaded && m_query.isArmed()) {
        updateMatches();
    }
    return loaded;
}

bool CompletionController::loadCatalogTsv(const QByteArray &tsv, QString *error)
{
    const bool loaded = m_catalog.loadTsv(tsv, error);
    if (loaded && m_query.isArmed()) {
        updateMatches();
    }
    return loaded;
}

bool CompletionController::loadUsage(const QString &path, QString *error)
{
    m_usagePath = path;
    return m_usage.load(path, error);
}

void CompletionController::setUsagePath(QString path)
{
    m_usagePath = std::move(path);
}

bool CompletionController::handleKey(const WaylandInputMethod::KeyEvent &event)
{
    if (!event.pressed) {
        return m_consumedKeys.remove(event.keycode);
    }

    if (m_inputMethod && m_inputMethod->isSensitive()) {
        reset();
        return false;
    }

    bool consumed = false;
    if (event.control || event.alt || event.super) {
        reset();
        return false;
    }

    switch (event.keysym) {
    case XKB_KEY_Escape:
        if (m_query.isArmed()) {
            dismiss();
            consumed = true;
        }
        break;
    case XKB_KEY_Up:
        if (m_visible) {
            moveSelection(-1);
            consumed = true;
        }
        break;
    case XKB_KEY_Down:
        if (m_visible) {
            moveSelection(1);
            consumed = true;
        }
        break;
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
    case XKB_KEY_Tab:
        if (m_visible) {
            select();
            consumed = true;
        }
        break;
    case XKB_KEY_BackSpace:
        if (m_query.isArmed()) {
            m_query.backspace();
            emit queryChanged();
            updateMatches();
        }
        break;
    default:
        if (event.text.size() == 1) {
            const QueryState::Change change = m_query.input(event.text.front());
            if (change == QueryState::Change::Armed) {
                emit queryChanged();
                updateMatches();
            } else if (change == QueryState::Change::Updated) {
                emit queryChanged();
                updateMatches();
            } else if (change == QueryState::Change::ExactRequested) {
                const QVector<EmojiMatch> exact = EmojiMatcher::match(
                    m_catalog, m_query.query(), m_usage, 1);
                if (!exact.isEmpty() && exact.first().tier == MatchTier::Exact) {
                    commitEntry(exact.first().entry);
                    consumed = true;
                } else {
                    m_query.cancel();
                    m_query.input(u':');
                    emit queryChanged();
                    updateMatches();
                }
            } else if (change == QueryState::Change::Cancelled) {
                emit queryChanged();
                updateMatches();
            }
        } else if (m_query.isArmed()) {
            reset();
        }
        break;
    }

    if (consumed) {
        m_consumedKeys.insert(event.keycode);
    }
    return consumed;
}

void CompletionController::setFallbackMode(bool enabled)
{
    if (m_fallbackMode == enabled) {
        return;
    }
    m_fallbackMode = enabled;
    reset();
}

CandidateModel *CompletionController::candidates()
{
    return &m_candidates;
}

bool CompletionController::isVisible() const
{
    return m_visible;
}

QString CompletionController::query() const
{
    return m_query.query().toString();
}

void CompletionController::moveSelection(int delta)
{
    const int count = m_candidates.rowCount();
    if (count == 0) {
        return;
    }
    int next = m_candidates.selectedIndex() + delta;
    if (next < 0) {
        next = count - 1;
    } else if (next >= count) {
        next = 0;
    }
    m_candidates.setSelectedIndex(next);
}

void CompletionController::select(int index)
{
    if (index >= 0) {
        m_candidates.setSelectedIndex(index);
    }
    const EmojiEntry *entry = m_candidates.selectedEntry();
    if (!entry) {
        return;
    }

    commitEntry(entry);
}

void CompletionController::commitEntry(const EmojiEntry *entry)
{
    if (!entry) {
        return;
    }

    const qsizetype shortcodeLength = m_query.shortcodeLength();
    const std::uint32_t eraseBytes = std::uint32_t(1 + m_query.query().toString().toUtf8().size());
    if (m_fallbackMode) {
        emit fallbackCommitRequested(int(shortcodeLength), entry->emoji, entry->alias);
    } else if (m_inputMethod) {
        m_inputMethod->deleteAndCommit(eraseBytes, entry->emoji);
    }
    if (!m_fallbackMode) {
        m_usage.record(entry->alias);
        if (!m_usagePath.isEmpty()) {
            m_usage.save(m_usagePath);
        }
        emit committed(entry->emoji, entry->alias);
    }
    reset();
}

void CompletionController::observeFallbackCharacter(QChar character)
{
    if (!m_fallbackMode) {
        return;
    }
    const QueryState::Change change = m_query.input(character);
    if (change == QueryState::Change::ExactRequested) {
        const QVector<EmojiMatch> exact = EmojiMatcher::match(
            m_catalog, m_query.query(), m_usage, 1);
        if (!exact.isEmpty() && exact.first().tier == MatchTier::Exact) {
            const int eraseCharacters = int(m_query.shortcodeLength(true));
            const EmojiEntry *entry = exact.first().entry;
            emit fallbackCommitRequested(eraseCharacters, entry->emoji, entry->alias);
            reset();
            return;
        }
        m_query.cancel();
        m_query.input(u':');
    }
    emit queryChanged();
    updateMatches();
}

void CompletionController::confirmFallbackCommit(const QString &emoji, const QString &alias)
{
    m_usage.record(alias);
    if (!m_usagePath.isEmpty()) {
        m_usage.save(m_usagePath);
    }
    emit committed(emoji, alias);
}

void CompletionController::observeFallbackBackspace()
{
    if (!m_fallbackMode || !m_query.isArmed()) {
        return;
    }
    m_query.backspace();
    emit queryChanged();
    updateMatches();
}

void CompletionController::dismiss()
{
    reset();
}

void CompletionController::preview(const QString &query)
{
    reset();
    m_query.input(u':');
    for (const QChar character : query) {
        if (m_query.input(character) != QueryState::Change::Updated) {
            break;
        }
    }
    emit queryChanged();
    updateMatches();
}

void CompletionController::demoInput(const QString &text)
{
    for (const QChar character : text) {
        const QueryState::Change change = m_query.input(character);
        if (change == QueryState::Change::ExactRequested) {
            const QVector<EmojiMatch> exact = EmojiMatcher::match(
                m_catalog, m_query.query(), m_usage, 1);
            if (!exact.isEmpty() && exact.first().tier == MatchTier::Exact) {
                m_candidates.setMatches(exact, 1);
                select(0);
                return;
            }
            m_query.cancel();
            m_query.input(u':');
        } else if (change == QueryState::Change::Cancelled) {
            break;
        }
    }
    emit queryChanged();
    updateMatches();
}

void CompletionController::demoBackspace()
{
    if (!m_query.isArmed()) {
        return;
    }
    m_query.backspace();
    emit queryChanged();
    updateMatches();
}

void CompletionController::updateMatches()
{
    if (!m_query.isArmed() || m_query.query().isEmpty()) {
        m_candidates.clear();
        setVisible(false);
        return;
    }
    const QVector<EmojiMatch> matches = EmojiMatcher::match(m_catalog, m_query.query(), m_usage, 8);
    m_candidates.setMatches(matches, 8);
    setVisible(!matches.isEmpty());
}

void CompletionController::reset()
{
    const bool queryChanged = m_query.isArmed() || !m_query.query().isEmpty();
    m_query.cancel();
    m_candidates.clear();
    setVisible(false);
    if (queryChanged) {
        emit this->queryChanged();
    }
}

void CompletionController::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;
    emit visibleChanged();
}
