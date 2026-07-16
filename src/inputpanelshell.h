// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtWaylandClient/private/qwaylandshellintegration_p.h>

class WaylandInputMethod;

class InputPanelShellIntegration final : public QtWaylandClient::QWaylandShellIntegration
{
public:
    explicit InputPanelShellIntegration(WaylandInputMethod *inputMethod);

    bool initialize(QtWaylandClient::QWaylandDisplay *display) override;
    QtWaylandClient::QWaylandShellSurface *createShellSurface(
        QtWaylandClient::QWaylandWindow *window) override;

private:
    WaylandInputMethod *m_inputMethod = nullptr;
};
