// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xwaylandfallback.h"

#include <QByteArray>
#include <QSocketNotifier>
#include <QTimer>

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XTest.h>
#include <xkbcommon/xkbcommon.h>

#include <poll.h>

#include <array>

namespace {
int ignoreXError(Display *, XErrorEvent *)
{
    return 0;
}
}

struct XWaylandFallback::Private
{
    explicit Private(XWaylandFallback *q)
        : q(q)
    {
    }

    ~Private()
    {
        if (display) {
            if (selectionWindow) {
                XDestroyWindow(display, selectionWindow);
            }
            XCloseDisplay(display);
        }
    }

    bool steamWindow(Window window)
    {
        XClassHint hint{};
        if (!XGetClassHint(display, window, &hint)) {
            return false;
        }
        const QString resourceClass = QString::fromLocal8Bit(hint.res_class ? hint.res_class : "");
        const QString resourceName = QString::fromLocal8Bit(hint.res_name ? hint.res_name : "");
        if (hint.res_class) {
            XFree(hint.res_class);
        }
        if (hint.res_name) {
            XFree(hint.res_name);
        }
        return resourceClass.compare(QStringLiteral("steam"), Qt::CaseInsensitive) == 0
            || resourceName.compare(QStringLiteral("steam"), Qt::CaseInsensitive) == 0
            || resourceName.compare(QStringLiteral("steamwebhelper"), Qt::CaseInsensitive) == 0;
    }

    bool activeSteamWindow(Window *window = nullptr)
    {
        if (!display) {
            return false;
        }
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long itemCount = 0;
        unsigned long bytesAfter = 0;
        unsigned char *data = nullptr;
        if (XGetWindowProperty(display, root, activeWindowAtom, 0, 1, False, XA_WINDOW,
                &actualType, &actualFormat, &itemCount, &bytesAfter, &data) != Success
            || !data || itemCount != 1) {
            if (data) {
                XFree(data);
            }
            return false;
        }
        const Window activeWindow = *reinterpret_cast<Window *>(data);
        XFree(data);

        const bool steam = steamWindow(activeWindow);
        if (steam && window) {
            *window = activeWindow;
        }
        return steam;
    }

    void resetTracking()
    {
        trackingQuery = false;
        targetPointValid = false;
        trackedCharacters = 0;
    }

    void updateNavigationGrabs()
    {
        if (!display || navigationGrabbed == (enabled && navigationActive)) {
            return;
        }
        static constexpr unsigned int modifiers[] = {
            0, LockMask, Mod2Mask, LockMask | Mod2Mask,
        };
        static constexpr KeySym keysyms[] = {
            XK_Up, XK_Down, XK_Return, XK_KP_Enter, XK_Escape,
        };
        for (const KeySym keysym : keysyms) {
            const KeyCode keycode = XKeysymToKeycode(display, keysym);
            for (const unsigned int modifier : modifiers) {
                if (enabled && navigationActive) {
                    XGrabKey(display, keycode, modifier, root, False,
                        GrabModeAsync, GrabModeAsync);
                } else {
                    XUngrabKey(display, keycode, modifier, root);
                }
            }
        }
        XSync(display, False);
        navigationGrabbed = enabled && navigationActive;
    }

    void processGrabbedKey(const XKeyEvent &event)
    {
        if (!enabled || !navigationActive) {
            return;
        }
        if (!activeSteamWindow()) {
            navigationActive = false;
            updateNavigationGrabs();
            XAllowEvents(display, ReplayKeyboard, event.time);
            XFlush(display);
            resetTracking();
            emit q->routeInvalidated();
            return;
        }
        const KeySym keysym = XLookupKeysym(const_cast<XKeyEvent *>(&event), 0);
        if (keysym == XK_Return || keysym == XK_KP_Enter) {
            if (event.type == KeyRelease) {
                emit q->selectionRequested();
            }
            return;
        }
        if (event.type != KeyPress) {
            return;
        }
        if (keysym == XK_Up) {
            emit q->navigationRequested(-1);
        } else if (keysym == XK_Down) {
            emit q->navigationRequested(1);
        } else if (keysym == XK_Escape) {
            emit q->dismissalRequested();
        }
    }

    void capturePointer()
    {
        Window activeWindow = None;
        if (lastClickValid && activeSteamWindow(&activeWindow) && activeWindow == lastClickWindow) {
            targetX = lastClickX;
            targetY = lastClickY;
            targetPointValid = true;
            targetWindow = activeWindow;
            return;
        }
        targetPointValid = false;
        targetWindow = None;
    }

    void processRawButton(const XIRawEvent *raw)
    {
        Window activeWindow = None;
        if (!enabled || raw->evtype != XI_RawButtonPress || raw->detail != 1
            || !activeSteamWindow(&activeWindow)) {
            return;
        }
        if (trackingQuery) {
            resetTracking();
            emit q->routeInvalidated();
        }
        Window rootReturn = None;
        Window childReturn = None;
        int windowX = 0;
        int windowY = 0;
        unsigned int mask = 0;
        lastClickValid = XQueryPointer(display, root, &rootReturn, &childReturn,
            &lastClickX, &lastClickY, &windowX, &windowY, &mask);
        lastClickWindow = activeWindow;
    }

    void processRawKey(const XIRawEvent *raw)
    {
        if (!enabled || raw->evtype != XI_RawKeyPress || !activeSteamWindow()) {
            if (enabled && !activeSteamWindow()) {
                resetTracking();
                emit q->routeInvalidated();
            }
            return;
        }

        XkbStateRec state{};
        if (XkbGetState(display, XkbUseCoreKbd, &state) != Success) {
            resetTracking();
            return;
        }
        if (state.mods & (ControlMask | Mod1Mask | Mod4Mask)) {
            resetTracking();
            return;
        }

        KeySym keysym = NoSymbol;
        unsigned int consumedModifiers = 0;
        if (!XkbLookupKeySym(display, static_cast<KeyCode>(raw->detail), state.mods,
                &consumedModifiers, &keysym)) {
            resetTracking();
            return;
        }
        if (navigationActive && (keysym == XK_Up || keysym == XK_Down
                || keysym == XK_Return || keysym == XK_KP_Enter || keysym == XK_Escape)) {
            return;
        }
        if (keysym == XK_BackSpace) {
            if (trackingQuery) {
                emit q->backspaceObserved();
                --trackedCharacters;
                if (trackedCharacters <= 0) {
                    resetTracking();
                }
            }
            return;
        }
        if (keysym == XK_Tab || keysym == XK_ISO_Left_Tab) {
            lastClickValid = false;
        }

        std::array<char, 8> text{};
        const int length = xkb_keysym_to_utf8(static_cast<xkb_keysym_t>(keysym),
            text.data(), text.size());
        if (length <= 1) {
            if (trackingQuery) {
                resetTracking();
            }
            return;
        }
        const QString input = QString::fromUtf8(text.data(), length - 1);
        if (input.size() != 1) {
            resetTracking();
            return;
        }

        const QChar character = input.front();
        if (character == u':') {
            const bool closingColon = trackingQuery;
            if (!closingColon) {
                capturePointer();
                trackingQuery = true;
                trackedCharacters = 1;
            }
            emit q->characterObserved(character);
            if (closingColon) {
                trackingQuery = false;
                trackedCharacters = 0;
            }
            return;
        }
        if (!trackingQuery) {
            return;
        }
        if (!character.isLetterOrNumber() && character != u'_'
            && character != u'+' && character != u'-') {
            resetTracking();
            emit q->characterObserved(character);
            return;
        }
        ++trackedCharacters;
        emit q->characterObserved(character);
    }

    void serveSelectionRequest(const XSelectionRequestEvent &request)
    {
        XSelectionEvent response{};
        response.type = SelectionNotify;
        response.display = request.display;
        response.requestor = request.requestor;
        response.selection = request.selection;
        response.target = request.target;
        response.time = request.time;
        response.property = None;
        const Atom property = request.property == None ? request.target : request.property;

        if (request.target == targetsAtom) {
            const Atom supported[] = {targetsAtom, utf8Atom};
            XChangeProperty(display, request.requestor, property, XA_ATOM, 32,
                PropModeReplace, reinterpret_cast<const unsigned char *>(supported), 2);
            response.property = property;
        } else if (request.target == utf8Atom) {
            const QByteArray utf8 = selectionText.toUtf8();
            XChangeProperty(display, request.requestor, property, utf8Atom, 8,
                PropModeReplace, reinterpret_cast<const unsigned char *>(utf8.constData()),
                utf8.size());
            response.property = property;
            if (insertionPending && selectionText == pendingEmoji
                && steamWindow(request.requestor)) {
                selectionServed = true;
            }
        }
        XSendEvent(display, request.requestor, False, 0,
            reinterpret_cast<XEvent *>(&response));
        XFlush(display);
    }

    void beginInsertion()
    {
        selectionServed = false;
        selectionText = pendingEmoji;
        XSetSelectionOwner(display, primaryAtom, selectionWindow, CurrentTime);
        XFlush(display);

        QTimer::singleShot(0, q, [this] {
            Window activeWindow = None;
            if (!enabled || !activeSteamWindow(&activeWindow) || activeWindow != targetWindow) {
                finishInsertion(false);
                return;
            }

            const KeyCode backspace = XKeysymToKeycode(display, XK_BackSpace);
            for (int index = 0; index < pendingEraseCharacters; ++index) {
                XTestFakeKeyEvent(display, backspace, True, CurrentTime);
                XTestFakeKeyEvent(display, backspace, False, CurrentTime);
            }

            Window rootReturn = None;
            Window childReturn = None;
            int originalX = 0;
            int originalY = 0;
            int windowX = 0;
            int windowY = 0;
            unsigned int mask = 0;
            XQueryPointer(display, root, &rootReturn, &childReturn,
                &originalX, &originalY, &windowX, &windowY, &mask);
            XTestFakeMotionEvent(display, -1, targetX, targetY, CurrentTime);
            XTestFakeButtonEvent(display, 2, True, CurrentTime);
            XTestFakeButtonEvent(display, 2, False, CurrentTime);
            XTestFakeMotionEvent(display, -1, originalX, originalY, CurrentTime);
            XFlush(display);

            QTimer::singleShot(500, q, [this] {
                finishInsertion(selectionServed);
            });
        });
    }

    void finishInsertion(bool committed)
    {
        const QString completedAlias = pendingAlias;
        const QString completedEmoji = pendingEmoji;
        if (display && XGetSelectionOwner(display, primaryAtom) == selectionWindow) {
            if (hadPreviousSelection) {
                selectionText = previousSelection;
            } else {
                selectionText.clear();
                XSetSelectionOwner(display, primaryAtom, None, CurrentTime);
                XFlush(display);
            }
        } else {
            selectionText.clear();
        }
        previousSelection.clear();
        pendingEmoji.clear();
        pendingAlias.clear();
        awaitingPreviousSelection = false;
        insertionPending = false;
        resetTracking();
        if (committed) {
            emit q->replacementCommitted(completedEmoji, completedAlias);
        }
    }

    void selectionReceived(const XSelectionEvent &event)
    {
        if (!awaitingPreviousSelection || event.selection != primaryAtom) {
            return;
        }
        awaitingPreviousSelection = false;
        bool captured = false;
        if (event.property != None) {
            Atom actualType = None;
            int actualFormat = 0;
            unsigned long itemCount = 0;
            unsigned long bytesAfter = 0;
            unsigned char *data = nullptr;
            if (XGetWindowProperty(display, selectionWindow, selectionPropertyAtom,
                    0, 1024 * 1024, True, utf8Atom, &actualType, &actualFormat,
                    &itemCount, &bytesAfter, &data) == Success
                && data && actualFormat == 8) {
                previousSelection = QString::fromUtf8(
                    reinterpret_cast<const char *>(data), itemCount);
                captured = true;
            }
            if (data) {
                XFree(data);
            }
        }
        if (captured) {
            beginInsertion();
        } else {
            finishInsertion(false);
        }
    }

    void processEvents()
    {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == SelectionRequest) {
                serveSelectionRequest(event.xselectionrequest);
            } else if (event.type == SelectionNotify) {
                selectionReceived(event.xselection);
            } else if (event.type == KeyPress || event.type == KeyRelease) {
                processGrabbedKey(event.xkey);
            } else if (event.type == PropertyNotify && event.xproperty.atom == activeWindowAtom) {
                const bool steamActive = activeSteamWindow();
                emit q->steamActiveChanged(steamActive);
                if (enabled && !steamActive) {
                    navigationActive = false;
                    updateNavigationGrabs();
                    resetTracking();
                    emit q->routeInvalidated();
                }
            } else if (event.type == GenericEvent && event.xcookie.extension == xiOpcode
                && XGetEventData(display, &event.xcookie)) {
                auto *raw = static_cast<XIRawEvent *>(event.xcookie.data);
                if (raw->evtype == XI_RawButtonPress) {
                    processRawButton(raw);
                } else {
                    processRawKey(raw);
                }
                XFreeEventData(display, &event.xcookie);
            }
        }
    }

    XWaylandFallback *q = nullptr;
    Display *display = nullptr;
    Window root = None;
    Window selectionWindow = None;
    Atom activeWindowAtom = None;
    Atom primaryAtom = None;
    Atom utf8Atom = None;
    Atom targetsAtom = None;
    Atom selectionPropertyAtom = None;
    int xiOpcode = 0;
    std::unique_ptr<QSocketNotifier> notifier;
    QString selectionText;
    QString previousSelection;
    QString pendingEmoji;
    QString pendingAlias;
    bool enabled = false;
    bool trackingQuery = false;
    bool targetPointValid = false;
    bool insertionPending = false;
    bool awaitingPreviousSelection = false;
    bool hadPreviousSelection = false;
    bool lastClickValid = false;
    bool navigationActive = false;
    bool navigationGrabbed = false;
    bool selectionServed = false;
    int pendingEraseCharacters = 0;
    int trackedCharacters = 0;
    int targetX = 0;
    int targetY = 0;
    int lastClickX = 0;
    int lastClickY = 0;
    Window lastClickWindow = None;
    Window targetWindow = None;
};

XWaylandFallback::XWaylandFallback(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>(this))
{
}

XWaylandFallback::~XWaylandFallback() = default;

bool XWaylandFallback::initialize(QString *error)
{
    XSetErrorHandler(ignoreXError);
    d->display = XOpenDisplay(nullptr);
    if (!d->display) {
        if (error) {
            *error = QStringLiteral("Cannot connect to XWayland");
        }
        return false;
    }

    int event = 0;
    int errorCode = 0;
    if (!XQueryExtension(d->display, "XInputExtension", &d->xiOpcode, &event, &errorCode)) {
        if (error) {
            *error = QStringLiteral("XWayland does not provide XI2");
        }
        return false;
    }
    int major = 2;
    int minor = 0;
    if (XIQueryVersion(d->display, &major, &minor) != Success) {
        if (error) {
            *error = QStringLiteral("XWayland XI2 version is insufficient");
        }
        return false;
    }
    int testMajor = 0;
    int testMinor = 0;
    int testEvent = 0;
    int testError = 0;
    if (!XTestQueryExtension(d->display, &testEvent, &testError, &testMajor, &testMinor)) {
        if (error) {
            *error = QStringLiteral("XWayland does not provide XTEST");
        }
        return false;
    }

    d->root = DefaultRootWindow(d->display);
    d->selectionWindow = XCreateSimpleWindow(d->display, d->root, 0, 0, 1, 1, 0, 0, 0);
    d->activeWindowAtom = XInternAtom(d->display, "_NET_ACTIVE_WINDOW", False);
    d->primaryAtom = XInternAtom(d->display, "PRIMARY", False);
    d->utf8Atom = XInternAtom(d->display, "UTF8_STRING", False);
    d->targetsAtom = XInternAtom(d->display, "TARGETS", False);
    d->selectionPropertyAtom = XInternAtom(d->display,
        "EMOJI_CORD_PREVIOUS_PRIMARY", False);

    unsigned char maskBytes[XIMaskLen(XI_LASTEVENT)]{};
    XIEventMask mask{XIAllMasterDevices, sizeof(maskBytes), maskBytes};
    XISetMask(mask.mask, XI_RawKeyPress);
    XISetMask(mask.mask, XI_RawButtonPress);
    if (XISelectEvents(d->display, d->root, &mask, 1) != Success) {
        if (error) {
            *error = QStringLiteral("Cannot observe XWayland keyboard events");
        }
        return false;
    }
    XSelectInput(d->display, d->root, PropertyChangeMask);
    XFlush(d->display);

    d->notifier = std::make_unique<QSocketNotifier>(ConnectionNumber(d->display),
        QSocketNotifier::Read, this);
    connect(d->notifier.get(), &QSocketNotifier::activated, this, [this] {
        d->processEvents();
    });
    if (error) {
        error->clear();
    }
    return true;
}

void XWaylandFallback::setEnabled(bool enabled)
{
    d->enabled = enabled;
    if (!enabled) {
        if (d->insertionPending) {
            d->finishInsertion(false);
        }
        d->resetTracking();
    }
    d->updateNavigationGrabs();
}

void XWaylandFallback::setNavigationActive(bool active)
{
    d->navigationActive = active;
    d->updateNavigationGrabs();
}

bool XWaylandFallback::isAvailable() const
{
    return d->display;
}

bool XWaylandFallback::isSteamActive() const
{
    return d->activeSteamWindow();
}

void XWaylandFallback::replaceShortcode(int eraseCharacters, const QString &emoji,
    const QString &alias)
{
    Window activeWindow = None;
    if (!d->enabled || d->insertionPending || !d->targetPointValid || emoji.isEmpty()
        || eraseCharacters <= 0 || !d->activeSteamWindow(&activeWindow)
        || activeWindow != d->targetWindow) {
        return;
    }

    d->insertionPending = true;
    d->pendingEraseCharacters = eraseCharacters;
    d->pendingEmoji = emoji;
    d->pendingAlias = alias;
    const Window previousOwner = XGetSelectionOwner(d->display, d->primaryAtom);
    d->hadPreviousSelection = previousOwner != None;
    d->previousSelection.clear();
    if (previousOwner == None) {
        d->beginInsertion();
        return;
    }
    if (previousOwner == d->selectionWindow) {
        d->previousSelection = d->selectionText;
        d->beginInsertion();
        return;
    }

    d->awaitingPreviousSelection = true;
    XConvertSelection(d->display, d->primaryAtom, d->utf8Atom,
        d->selectionPropertyAtom, d->selectionWindow, CurrentTime);
    XFlush(d->display);
    QTimer::singleShot(40, this, [this] {
        if (d->awaitingPreviousSelection) {
            d->awaitingPreviousSelection = false;
            d->finishInsertion(false);
        }
    });
}
