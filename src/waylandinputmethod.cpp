// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "waylandinputmethod.h"

#include "input-method-unstable-v1-client-protocol.h"

#include <QTimer>

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <utility>

namespace {
constexpr std::uint32_t passwordPurpose = 8;
constexpr std::uint32_t pinPurpose = 13;
constexpr std::uint32_t hiddenTextHint = 1U << 6;
constexpr std::uint32_t sensitiveDataHint = 1U << 7;
}

struct WaylandInputMethod::Private {
    WaylandInputMethod *owner = nullptr;
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    zwp_input_method_v1 *inputMethod = nullptr;
    zwp_input_panel_v1 *inputPanel = nullptr;
    zwp_input_method_context_v1 *context = nullptr;
    wl_keyboard *keyboard = nullptr;
    xkb_context *xkbContext = nullptr;
    xkb_keymap *keymap = nullptr;
    xkb_state *xkbState = nullptr;
    KeyHandler handler;
    std::uint32_t commitSerial = 0;
    bool sensitive = false;
    QTimer repeatTimer;
    KeyEvent repeatEvent;
    std::uint32_t repeatTime = 0;
    std::int32_t repeatRate = 0;
    std::int32_t repeatDelay = 0;

    void clearKeyboard()
    {
        repeatTimer.stop();
        if (keyboard) {
            wl_keyboard_destroy(keyboard);
            keyboard = nullptr;
        }
        if (xkbState) {
            xkb_state_unref(xkbState);
            xkbState = nullptr;
        }
        if (keymap) {
            xkb_keymap_unref(keymap);
            keymap = nullptr;
        }
    }

    void clearContext()
    {
        clearKeyboard();
        if (context) {
            zwp_input_method_context_v1_destroy(context);
            context = nullptr;
        }
        sensitive = false;
        commitSerial = 0;
    }

    static void registryGlobal(void *data, wl_registry *registry, std::uint32_t name,
        const char *interface, std::uint32_t version)
    {
        auto *self = static_cast<Private *>(data);
        if (qstrcmp(interface, zwp_input_method_v1_interface.name) == 0) {
            self->inputMethod = static_cast<zwp_input_method_v1 *>(
                wl_registry_bind(registry, name, &zwp_input_method_v1_interface, std::min(version, 1U)));
        } else if (qstrcmp(interface, zwp_input_panel_v1_interface.name) == 0) {
            self->inputPanel = static_cast<zwp_input_panel_v1 *>(
                wl_registry_bind(registry, name, &zwp_input_panel_v1_interface, std::min(version, 1U)));
        }
    }

    static void registryGlobalRemove(void *, wl_registry *, std::uint32_t) { }

    static void activate(void *data, zwp_input_method_v1 *, zwp_input_method_context_v1 *context)
    {
        auto *self = static_cast<Private *>(data);
        self->clearContext();
        self->context = context;
        static const zwp_input_method_context_v1_listener contextListener{
            surroundingText, reset, contentType, invokeAction, commitState, preferredLanguage};
        zwp_input_method_context_v1_add_listener(context, &contextListener, self);

        self->keyboard = zwp_input_method_context_v1_grab_keyboard(context);
        static const wl_keyboard_listener keyboardListener{
            keyboardKeymap, keyboardEnter, keyboardLeave, keyboardKey, keyboardModifiers,
            keyboardRepeatInfo};
        wl_keyboard_add_listener(self->keyboard, &keyboardListener, self);
        emit self->owner->contextActivated();
    }

    static void deactivate(void *data, zwp_input_method_v1 *, zwp_input_method_context_v1 *context)
    {
        auto *self = static_cast<Private *>(data);
        if (self->context == context) {
            self->clearContext();
            emit self->owner->contextDeactivated();
        }
    }

    static void surroundingText(void *data, zwp_input_method_context_v1 *, const char *text,
        std::uint32_t cursor, std::uint32_t anchor)
    {
        emit static_cast<Private *>(data)->owner->surroundingTextChanged(
            QString::fromUtf8(text), cursor, anchor);
    }
    static void reset(void *data, zwp_input_method_context_v1 *)
    {
        emit static_cast<Private *>(data)->owner->resetRequested();
    }
    static void invokeAction(void *, zwp_input_method_context_v1 *, std::uint32_t, std::uint32_t) { }
    static void preferredLanguage(void *, zwp_input_method_context_v1 *, const char *) { }

    static void contentType(void *data, zwp_input_method_context_v1 *, std::uint32_t hint,
        std::uint32_t purpose)
    {
        auto *self = static_cast<Private *>(data);
        const bool sensitive = (hint & (hiddenTextHint | sensitiveDataHint)) != 0
            || purpose == passwordPurpose || purpose == pinPurpose;
        if (self->sensitive != sensitive) {
            self->sensitive = sensitive;
            emit self->owner->sensitiveChanged(sensitive);
        }
    }

    static void commitState(void *data, zwp_input_method_context_v1 *, std::uint32_t serial)
    {
        static_cast<Private *>(data)->commitSerial = serial;
    }

    static void keyboardKeymap(void *data, wl_keyboard *, std::uint32_t format, int fd,
        std::uint32_t size)
    {
        auto *self = static_cast<Private *>(data);
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || size == 0) {
            close(fd);
            return;
        }

        void *mapping = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (mapping == MAP_FAILED) {
            return;
        }
        xkb_keymap *newKeymap = xkb_keymap_new_from_string(self->xkbContext,
            static_cast<const char *>(mapping), XKB_KEYMAP_FORMAT_TEXT_V1,
            XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(mapping, size);
        if (!newKeymap) {
            return;
        }

        if (self->xkbState) {
            xkb_state_unref(self->xkbState);
        }
        if (self->keymap) {
            xkb_keymap_unref(self->keymap);
        }
        self->keymap = newKeymap;
        self->xkbState = xkb_state_new(newKeymap);
    }

    static void keyboardEnter(void *, wl_keyboard *, std::uint32_t, wl_surface *, wl_array *) { }
    static void keyboardLeave(void *, wl_keyboard *, std::uint32_t, wl_surface *) { }

    static void keyboardKey(void *data, wl_keyboard *, std::uint32_t serial, std::uint32_t time,
        std::uint32_t key, std::uint32_t state)
    {
        auto *self = static_cast<Private *>(data);
        const bool pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
        KeyEvent event;
        event.keycode = key;
        event.pressed = pressed;

        if (self->xkbState) {
            const xkb_keycode_t xkbKey = key + 8;
            event.keysym = xkb_state_key_get_one_sym(self->xkbState, xkbKey);
            char text[64]{};
            const int length = xkb_state_key_get_utf8(self->xkbState, xkbKey, text, sizeof(text));
            if (length > 0) {
                event.text = QString::fromUtf8(text, length);
            }
            event.control = xkb_state_mod_name_is_active(
                                self->xkbState, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE)
                > 0;
            event.alt = xkb_state_mod_name_is_active(
                            self->xkbState, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE)
                > 0;
            event.super = xkb_state_mod_name_is_active(
                              self->xkbState, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE)
                > 0;
        }

        const bool handled = self->handler && self->handler(event);
        if (!handled && self->context) {
            zwp_input_method_context_v1_key(self->context, serial, time, key, state);
        }

        if (pressed && self->repeatRate > 0 && self->keymap
            && xkb_keymap_key_repeats(self->keymap, key + 8)) {
            self->repeatEvent = event;
            self->repeatEvent.autoRepeat = true;
            self->repeatTime = time;
            self->repeatTimer.start(self->repeatDelay);
        } else if (!pressed && self->repeatEvent.keycode == key) {
            self->repeatTimer.stop();
            self->repeatEvent = {};
        }
    }

    static void keyboardModifiers(void *data, wl_keyboard *, std::uint32_t serial,
        std::uint32_t depressed, std::uint32_t latched, std::uint32_t locked, std::uint32_t group)
    {
        auto *self = static_cast<Private *>(data);
        if (self->xkbState) {
            xkb_state_update_mask(self->xkbState, depressed, latched, locked, 0, 0, group);
        }
        if (self->context) {
            zwp_input_method_context_v1_modifiers(
                self->context, serial, depressed, latched, locked, group);
        }
    }

    static void keyboardRepeatInfo(void *data, wl_keyboard *, std::int32_t rate, std::int32_t delay)
    {
        auto *self = static_cast<Private *>(data);
        self->repeatRate = std::max(rate, 0);
        self->repeatDelay = std::max(delay, 0);
        if (self->repeatRate == 0) {
            self->repeatTimer.stop();
        }
    }

    void repeat()
    {
        if (!context || repeatRate <= 0 || !repeatEvent.pressed) {
            repeatTimer.stop();
            return;
        }
        const bool handled = handler && handler(repeatEvent);
        if (!handled) {
            zwp_input_method_context_v1_keysym(context, commitSerial, repeatTime++,
                repeatEvent.keysym, WL_KEYBOARD_KEY_STATE_PRESSED, 0);
            zwp_input_method_context_v1_keysym(context, commitSerial, repeatTime++,
                repeatEvent.keysym, WL_KEYBOARD_KEY_STATE_RELEASED, 0);
        }
        repeatTimer.start(std::max(1, 1000 / repeatRate));
        wl_display_flush(display);
    }
};

WaylandInputMethod::WaylandInputMethod(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->owner = this;
    d->xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    d->repeatTimer.setSingleShot(true);
    connect(&d->repeatTimer, &QTimer::timeout, this, [this] {
        d->repeat();
    });
}

WaylandInputMethod::~WaylandInputMethod()
{
    d->clearContext();
    if (d->inputPanel) {
        zwp_input_panel_v1_destroy(d->inputPanel);
    }
    if (d->inputMethod) {
        zwp_input_method_v1_destroy(d->inputMethod);
    }
    if (d->registry) {
        wl_registry_destroy(d->registry);
    }
    if (d->xkbContext) {
        xkb_context_unref(d->xkbContext);
    }
}

bool WaylandInputMethod::connectToCompositor(wl_display *display, QString *error)
{
    if (d->display) {
        return d->inputMethod != nullptr;
    }

    d->display = display;
    if (!d->display) {
        if (error) {
            *error = QStringLiteral("Cannot connect to the Wayland compositor");
        }
        return false;
    }

    d->registry = wl_display_get_registry(d->display);
    static const wl_registry_listener registryListener{
        Private::registryGlobal, Private::registryGlobalRemove};
    wl_registry_add_listener(d->registry, &registryListener, d.get());
    wl_display_roundtrip(d->display);

    if (!d->inputMethod) {
        if (error) {
            *error = QStringLiteral("KWin did not provide the privileged input-method global");
        }
        return false;
    }

    static const zwp_input_method_v1_listener inputMethodListener{
        Private::activate, Private::deactivate};
    zwp_input_method_v1_add_listener(d->inputMethod, &inputMethodListener, d.get());
    wl_display_flush(d->display);
    if (error) {
        error->clear();
    }
    return true;
}

bool WaylandInputMethod::isAvailable() const
{
    return d->inputMethod != nullptr;
}

bool WaylandInputMethod::hasInputPanel() const
{
    return d->inputPanel != nullptr;
}

bool WaylandInputMethod::hasActiveContext() const
{
    return d->context != nullptr;
}

bool WaylandInputMethod::isSensitive() const
{
    return d->sensitive;
}

void WaylandInputMethod::setKeyHandler(KeyHandler handler)
{
    d->handler = std::move(handler);
}

void WaylandInputMethod::deleteAndCommit(std::uint32_t beforeBytes, const QString &text)
{
    if (!d->context) {
        return;
    }
    zwp_input_method_context_v1_delete_surrounding_text(
        d->context, -static_cast<std::int32_t>(beforeBytes), beforeBytes);
    commit(text);
}

void WaylandInputMethod::commit(const QString &text)
{
    if (!d->context) {
        return;
    }
    const QByteArray utf8 = text.toUtf8();
    zwp_input_method_context_v1_commit_string(d->context, d->commitSerial, utf8.constData());
    wl_display_flush(d->display);
}

zwp_input_panel_surface_v1 *WaylandInputMethod::createOverlaySurface(wl_surface *surface)
{
    if (!d->inputPanel || !surface) {
        return nullptr;
    }
    return zwp_input_panel_v1_get_input_panel_surface(d->inputPanel, surface);
}
