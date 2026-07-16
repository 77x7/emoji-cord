// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "candidatemodel.h"
#include "emojicatalog.h"
#include "querystate.h"
#include "usagestore.h"
#include "waylandinputmethod.h"

#include <QObject>
#include <QSet>

class CompletionController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CandidateModel *candidates READ candidates CONSTANT)
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString query READ query NOTIFY queryChanged)

public:
    explicit CompletionController(WaylandInputMethod *inputMethod, QObject *parent = nullptr);
    ~CompletionController() override;

    bool loadCatalog(const QByteArray &json, QString *error = nullptr);
    bool loadCatalogTsv(const QByteArray &tsv, QString *error = nullptr);
    bool loadUsage(const QString &path, QString *error = nullptr);
    void setUsagePath(QString path);
    bool handleKey(const WaylandInputMethod::KeyEvent &event);
    void setFallbackMode(bool enabled);

    CandidateModel *candidates();
    bool isVisible() const;
    bool isFallbackMode() const;
    QString query() const;

    Q_INVOKABLE void moveSelection(int delta);
    Q_INVOKABLE void select(int index = -1);
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void preview(const QString &query);
    Q_INVOKABLE void demoInput(const QString &text);
    Q_INVOKABLE void demoBackspace();

public slots:
    void observeFallbackCharacter(QChar character);
    void observeFallbackBackspace();
    void confirmFallbackCommit(const QString &emoji, const QString &alias);

signals:
    void visibleChanged();
    void queryChanged();
    void committed(const QString &emoji, const QString &alias);
    void fallbackCommitRequested(int eraseCharacters, const QString &emoji,
        const QString &alias);
    void fallbackModeChanged();

private:
    void updateMatches();
    void commitEntry(const EmojiEntry *entry);
    void restoreFromSurroundingText(const QString &text, std::uint32_t cursor,
        std::uint32_t anchor);
    void reset();
    void setVisible(bool visible);

    WaylandInputMethod *m_inputMethod = nullptr;
    EmojiCatalog m_catalog;
    UsageStore m_usage;
    QueryState m_query;
    CandidateModel m_candidates;
    QSet<std::uint32_t> m_consumedKeys;
    QString m_usagePath;
    bool m_visible = false;
    bool m_fallbackMode = false;
};
