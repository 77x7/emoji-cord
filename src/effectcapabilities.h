// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QTimer>

class EffectCapabilities final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool blurAvailable READ blurAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(bool contrastAvailable READ contrastAvailable NOTIFY availabilityChanged)

public:
    explicit EffectCapabilities(QObject *parent = nullptr);

    bool blurAvailable() const;
    bool contrastAvailable() const;

public slots:
    void refresh();

signals:
    void availabilityChanged();

private:
    QTimer m_refreshTimer;
    bool m_blurAvailable = false;
    bool m_contrastAvailable = false;
};
