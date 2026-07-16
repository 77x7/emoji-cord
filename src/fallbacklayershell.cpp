// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fallbacklayershell.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

#include <wayland-client.h>

#include <algorithm>
#include <cstring>

class FallbackLayerSurface final : public QtWaylandClient::QWaylandShellSurface
{
public:
    FallbackLayerSurface(FallbackLayerShellIntegration *integration,
        zwlr_layer_shell_v1 *shell,
        QtWaylandClient::QWaylandWindow *window, const QPoint &position)
        : QWaylandShellSurface(window)
        , m_integration(integration)
        , m_window(window)
    {
        m_surface = zwlr_layer_shell_v1_get_layer_surface(shell, window->wlSurface(), nullptr,
            ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "emoji-cord-fallback");
        static const zwlr_layer_surface_v1_listener listener = {
            &FallbackLayerSurface::configured,
            &FallbackLayerSurface::closed,
        };
        zwlr_layer_surface_v1_add_listener(m_surface, &listener, this);
        zwlr_layer_surface_v1_set_anchor(m_surface,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
        zwlr_layer_surface_v1_set_exclusive_zone(m_surface, -1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(m_surface,
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
        setPosition(position);
        setWindowSize(window->windowContentGeometry().size());
    }

    ~FallbackLayerSurface() override
    {
        m_integration->clearSurface(this);
        if (m_surface) {
            zwlr_layer_surface_v1_destroy(m_surface);
        }
    }

    bool isExposed() const override { return m_configured; }

    void applyConfigure() override
    {
        if (!m_pendingSize.isEmpty()) {
            resizeFromApplyConfigure(m_pendingSize);
        }
    }

    void setWindowSize(const QSize &size) override
    {
        if (!m_surface || size.isEmpty()) {
            return;
        }
        m_requestedSize = size;
        zwlr_layer_surface_v1_set_size(m_surface, size.width(), size.height());
    }

    void setPosition(const QPoint &position)
    {
        if (!m_surface) {
            return;
        }
        m_position = position;
        zwlr_layer_surface_v1_set_margin(m_surface, position.y(), 0, 0, position.x());
        if (m_configured) {
            wl_surface_commit(wlSurface());
        }
    }

private:
    static void configured(void *data, zwlr_layer_surface_v1 *surface,
        std::uint32_t serial, std::uint32_t width, std::uint32_t height)
    {
        auto *self = static_cast<FallbackLayerSurface *>(data);
        zwlr_layer_surface_v1_ack_configure(surface, serial);
        self->m_pendingSize = QSize(width ? int(width) : self->m_requestedSize.width(),
            height ? int(height) : self->m_requestedSize.height());
        if (!self->m_configured) {
            self->m_configured = true;
            self->applyConfigure();
            self->m_window->updateExposure();
        } else {
            self->applyConfigureWhenPossible();
        }
    }

    static void closed(void *data, zwlr_layer_surface_v1 *)
    {
        static_cast<FallbackLayerSurface *>(data)->window()->window()->hide();
    }

    FallbackLayerShellIntegration *m_integration = nullptr;
    QtWaylandClient::QWaylandWindow *m_window = nullptr;
    zwlr_layer_surface_v1 *m_surface = nullptr;
    QSize m_requestedSize;
    QSize m_pendingSize;
    QPoint m_position;
    bool m_configured = false;
};

FallbackLayerShellIntegration::FallbackLayerShellIntegration() = default;

FallbackLayerShellIntegration::~FallbackLayerShellIntegration()
{
    if (m_layerShell) {
        if (zwlr_layer_shell_v1_get_version(m_layerShell) >= 3) {
            zwlr_layer_shell_v1_destroy(m_layerShell);
        } else {
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(m_layerShell));
        }
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
    }
    if (m_queue) {
        wl_event_queue_destroy(m_queue);
    }
}

bool FallbackLayerShellIntegration::initialize(QtWaylandClient::QWaylandDisplay *display)
{
    wl_display *nativeDisplay = display->wl_display();
    m_queue = wl_display_create_queue(nativeDisplay);
    m_registry = wl_display_get_registry(nativeDisplay);
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(m_registry), m_queue);
    static const wl_registry_listener listener = {
        &FallbackLayerShellIntegration::registryGlobal,
        &FallbackLayerShellIntegration::registryGlobalRemove,
    };
    wl_registry_add_listener(m_registry, &listener, this);
    wl_display_roundtrip_queue(nativeDisplay, m_queue);
    if (m_layerShell) {
        wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(m_layerShell), nullptr);
    }
    wl_registry_destroy(m_registry);
    m_registry = nullptr;
    wl_event_queue_destroy(m_queue);
    m_queue = nullptr;
    return m_layerShell;
}

QtWaylandClient::QWaylandShellSurface *FallbackLayerShellIntegration::createShellSurface(
    QtWaylandClient::QWaylandWindow *window)
{
    m_surface = new FallbackLayerSurface(this, m_layerShell, window, m_position);
    return m_surface;
}

void FallbackLayerShellIntegration::clearSurface(FallbackLayerSurface *surface)
{
    if (m_surface == surface) {
        m_surface = nullptr;
    }
}

void FallbackLayerShellIntegration::setPosition(const QPoint &position)
{
    m_position = position;
    if (m_surface) {
        m_surface->setPosition(position);
    }
}

void FallbackLayerShellIntegration::registryGlobal(void *data, wl_registry *registry,
    std::uint32_t name, const char *interface, std::uint32_t version)
{
    auto *self = static_cast<FallbackLayerShellIntegration *>(data);
    if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        self->m_layerShell = static_cast<zwlr_layer_shell_v1 *>(wl_registry_bind(registry,
            name, &zwlr_layer_shell_v1_interface, std::min(version, std::uint32_t(4))));
        wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(self->m_layerShell), self->m_queue);
    }
}
