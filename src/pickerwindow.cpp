// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pickerwindow.h"

#include "appsettings.h"
#include "completioncontroller.h"
#include "fallbacklayershell.h"
#include "inputpanelshell.h"
#include "waylandinputmethod.h"

#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QQmlContext>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

#include <algorithm>

PickerWindow::PickerWindow(CompletionController *controller, AppSettings *settings,
    WaylandInputMethod *inputMethod, Mode mode, QWindow *parent)
    : QQuickView(parent)
    , m_controller(controller)
    , m_settings(settings)
    , m_inputMethod(inputMethod)
    , m_mode(mode)
{
    setTitle(QStringLiteral("Emoji-cord"));
    setColor(Qt::transparent);
    if (m_settings) {
        connect(m_settings, &AppSettings::visibleSuggestionsChanged,
            this, [this] { updateGeometry(); });
    }
    setResizeMode(QQuickView::SizeRootObjectToView);

    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint;
    if (m_mode != Mode::Demo) {
        flags |= Qt::WindowDoesNotAcceptFocus;
    }
    setFlags(flags);
    rootContext()->setContextProperty(QStringLiteral("completionController"), m_controller);
    setSource(QUrl(QStringLiteral("qrc:/qml/Picker.qml")));

    connect(m_controller, &CompletionController::visibleChanged, this, [this] {
        updateGeometry();
        updateVisibility();
    });
    connect(m_controller, &CompletionController::fallbackModeChanged,
        this, &PickerWindow::updateVisibility);
    connect(m_controller->candidates(), &QAbstractItemModel::modelReset,
        this, &PickerWindow::updateGeometry);
}

PickerWindow::~PickerWindow()
{
    destroy();
}

void PickerWindow::setFallbackPosition(const QPoint &globalPosition)
{
    if (m_mode != Mode::Fallback || !m_fallbackShellIntegration) {
        return;
    }
    m_fallbackGlobalPosition = globalPosition;
    m_fallbackPositionValid = true;
    updateFallbackLayerPosition();
    updateVisibility();
}

void PickerWindow::clearFallbackPosition()
{
    if (m_mode != Mode::Fallback) {
        return;
    }
    m_fallbackPositionValid = false;
    updateVisibility();
}

void PickerWindow::updateFallbackLayerPosition()
{
    if (!m_fallbackPositionValid || !m_fallbackShellIntegration) {
        return;
    }
    QScreen *screen = QGuiApplication::screenAt(m_fallbackGlobalPosition);
    const QRect screenGeometry = screen ? screen->geometry() : QRect();
    QPoint localPosition = m_fallbackGlobalPosition - screenGeometry.topLeft();
    localPosition.setX(std::clamp(localPosition.x(), 0,
        std::max(0, screenGeometry.width() - width())));
    localPosition.setY(std::clamp(localPosition.y(), 0,
        std::max(0, screenGeometry.height() - height())));
    m_fallbackShellIntegration->setPosition(localPosition);
}

bool PickerWindow::initialize(QString *error)
{
    if (status() == QQuickView::Error) {
        if (error) {
            *error = errors().isEmpty() ? QStringLiteral("Cannot load picker QML")
                                        : errors().first().toString();
        }
        return false;
    }

    updateGeometry();
    if (m_mode == Mode::Demo) {
        if (error) {
            error->clear();
        }
        return true;
    }

    create();
    auto *waylandWindow = dynamic_cast<QtWaylandClient::QWaylandWindow *>(handle());
    if (!waylandWindow) {
        if (error) {
            *error = QStringLiteral("Picker did not create a Qt Wayland window");
        }
        return false;
    }

    if (m_mode == Mode::InputPanel) {
        m_shellIntegration = std::make_unique<InputPanelShellIntegration>(m_inputMethod);
    } else {
        m_fallbackShellIntegration = std::make_unique<FallbackLayerShellIntegration>();
    }
    QtWaylandClient::QWaylandShellIntegration *integration = m_mode == Mode::InputPanel
        ? static_cast<QtWaylandClient::QWaylandShellIntegration *>(m_shellIntegration.get())
        : static_cast<QtWaylandClient::QWaylandShellIntegration *>(m_fallbackShellIntegration.get());
    if (!integration->initialize(waylandWindow->display())) {
        if (error) {
            *error = m_mode == Mode::InputPanel
                ? QStringLiteral("KWin input-panel protocol is unavailable")
                : QStringLiteral("KWin layer-shell protocol is unavailable");
        }
        return false;
    }
    waylandWindow->setShellIntegration(integration);
    if (error) {
        error->clear();
    }
    return true;
}

void PickerWindow::updateVisibility()
{
    const bool routeMatches = m_mode == Mode::Demo
        || (m_mode == Mode::InputPanel && !m_controller->isFallbackMode())
        || (m_mode == Mode::Fallback && m_controller->isFallbackMode()
            && m_fallbackPositionValid);
    if (m_controller->isVisible() && routeMatches) {
        if (m_mode == Mode::Demo) {
            setPosition(QCursor::pos() + QPoint(16, 18));
        }
        show();
        if (m_mode == Mode::Demo) {
            requestActivate();
        }
    } else {
        hide();
    }
}

void PickerWindow::updateGeometry()
{
    constexpr int rowHeight = 38;
    constexpr int verticalMargin = 8;
    const int requestedRows = m_settings
        ? m_settings->visibleSuggestions() : AppSettings::defaultVisibleSuggestions;
    QScreen *targetScreen = screen();
    if (m_mode == Mode::Fallback && m_fallbackPositionValid) {
        targetScreen = QGuiApplication::screenAt(m_fallbackGlobalPosition);
    }
    const int availableHeight = targetScreen ? targetScreen->availableGeometry().height() : 480;
    const int screenRows = std::max(1, (availableHeight - 80) / rowHeight);
    const int visibleRows = std::min({m_controller->candidates()->rowCount(),
        requestedRows, screenRows});
    const QSize pickerSize(280,
        verticalMargin + visibleRows * rowHeight);
    setMinimumSize(pickerSize);
    setMaximumSize(pickerSize);
    resize(pickerSize);
    if (m_mode == Mode::Fallback) {
        updateFallbackLayerPosition();
    }
}
