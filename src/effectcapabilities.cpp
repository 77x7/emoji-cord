// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "effectcapabilities.h"

#include <KWindowEffects>

EffectCapabilities::EffectCapabilities(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setInterval(1000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &EffectCapabilities::refresh);
    m_refreshTimer.start();
    QTimer::singleShot(0, this, &EffectCapabilities::refresh);
}

bool EffectCapabilities::blurAvailable() const
{
    return m_blurAvailable;
}

bool EffectCapabilities::contrastAvailable() const
{
    return m_contrastAvailable;
}

void EffectCapabilities::refresh()
{
    const bool blur = KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
    const bool contrast = KWindowEffects::isEffectAvailable(KWindowEffects::BackgroundContrast);
    if (m_blurAvailable == blur && m_contrastAvailable == contrast) {
        return;
    }
    m_blurAvailable = blur;
    m_contrastAvailable = contrast;
    emit availabilityChanged();
}
