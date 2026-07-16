// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtWaylandClient/private/qwaylandshellintegration_p.h>

#include <QPoint>

struct wl_event_queue;
struct wl_registry;
struct zwlr_layer_shell_v1;

class FallbackLayerSurface;

class FallbackLayerShellIntegration final : public QtWaylandClient::QWaylandShellIntegration
{
public:
    FallbackLayerShellIntegration();
    ~FallbackLayerShellIntegration() override;

    bool initialize(QtWaylandClient::QWaylandDisplay *display) override;
    QtWaylandClient::QWaylandShellSurface *createShellSurface(
        QtWaylandClient::QWaylandWindow *window) override;
    void setPosition(const QPoint &position);
    void clearSurface(FallbackLayerSurface *surface);

private:
    static void registryGlobal(void *data, wl_registry *registry, std::uint32_t name,
        const char *interface, std::uint32_t version);
    static void registryGlobalRemove(void *, wl_registry *, std::uint32_t) {}

    wl_event_queue *m_queue = nullptr;
    wl_registry *m_registry = nullptr;
    zwlr_layer_shell_v1 *m_layerShell = nullptr;
    FallbackLayerSurface *m_surface = nullptr;
    QPoint m_position;
};
