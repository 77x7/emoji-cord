// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>

struct wl_display;
struct wl_keyboard;
struct wl_registry;
struct wl_surface;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct zwp_input_method_context_v1;
struct zwp_input_method_v1;
struct zwp_input_panel_v1;
struct zwp_input_panel_surface_v1;

class QSocketNotifier;

class WaylandInputMethod final : public QObject
{
    Q_OBJECT

public:
    struct KeyEvent {
        std::uint32_t keycode = 0;
        std::uint32_t keysym = 0;
        QString text;
        bool pressed = false;
        bool autoRepeat = false;
        bool control = false;
        bool alt = false;
        bool super = false;
    };

    using KeyHandler = std::function<bool(const KeyEvent &)>;

    explicit WaylandInputMethod(QObject *parent = nullptr);
    ~WaylandInputMethod() override;

    bool connectToCompositor(wl_display *display, QString *error = nullptr);
    bool isAvailable() const;
    bool hasInputPanel() const;
    bool hasActiveContext() const;
    bool isSensitive() const;

    void setKeyHandler(KeyHandler handler);
    void deleteAndCommit(std::uint32_t beforeBytes, const QString &text);
    void commit(const QString &text);
    zwp_input_panel_surface_v1 *createOverlaySurface(wl_surface *surface);

signals:
    void contextActivated();
    void contextDeactivated();
    void resetRequested();
    void sensitiveChanged(bool sensitive);

private:
    struct Private;
    std::unique_ptr<Private> d;
};
