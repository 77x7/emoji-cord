// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "inputpanelshell.h"

#include "input-method-unstable-v1-client-protocol.h"
#include "waylandinputmethod.h"

#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

namespace {
class InputPanelSurface final : public QtWaylandClient::QWaylandShellSurface
{
public:
    InputPanelSurface(zwp_input_panel_surface_v1 *surface,
        QtWaylandClient::QWaylandWindow *window)
        : QWaylandShellSurface(window)
        , m_surface(surface)
    {
        applyConfigureWhenPossible();
    }

    ~InputPanelSurface() override
    {
        if (m_surface) {
            zwp_input_panel_surface_v1_destroy(m_surface);
        }
    }

    void applyConfigure() override
    {
        if (m_surface) {
            zwp_input_panel_surface_v1_set_overlay_panel(m_surface);
        }
    }

private:
    zwp_input_panel_surface_v1 *m_surface = nullptr;
};
}

InputPanelShellIntegration::InputPanelShellIntegration(WaylandInputMethod *inputMethod)
    : m_inputMethod(inputMethod)
{
}

bool InputPanelShellIntegration::initialize(QtWaylandClient::QWaylandDisplay *)
{
    return m_inputMethod && m_inputMethod->isAvailable() && m_inputMethod->hasInputPanel();
}

QtWaylandClient::QWaylandShellSurface *InputPanelShellIntegration::createShellSurface(
    QtWaylandClient::QWaylandWindow *window)
{
    zwp_input_panel_surface_v1 *surface =
        m_inputMethod->createOverlaySurface(window->wlSurface());
    return surface ? new InputPanelSurface(surface, window) : nullptr;
}
