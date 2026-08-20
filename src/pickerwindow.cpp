// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pickerwindow.h"

#include "appsettings.h"
#include "completioncontroller.h"
#include "effectcapabilities.h"
#include "fallbacklayershell.h"
#include "inputpanelshell.h"
#include "waylandinputmethod.h"

#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainterPath>
#include <QScreen>
#include <QQmlContext>
#include <KWindowEffects>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

#include <algorithm>

PickerWindow::PickerWindow(CompletionController *controller, AppSettings *settings,
    EffectCapabilities *effectCapabilities, WaylandInputMethod *inputMethod,
    Mode mode, QWindow *parent)
    : QQuickView(parent)
    , m_controller(controller)
    , m_settings(settings)
    , m_effectCapabilities(effectCapabilities)
    , m_inputMethod(inputMethod)
    , m_mode(mode)
{
    setTitle(QStringLiteral("Emoji-cord"));
    setColor(Qt::transparent);
    if (m_settings) {
        connect(m_settings, &AppSettings::visibleSuggestionsChanged,
            this, [this] { updateGeometry(); });
        connect(m_settings, &AppSettings::blurEnabledChanged,
            this, [this] { applyEffects(); });
        connect(m_settings, &AppSettings::contrastEnabledChanged,
            this, [this] { applyEffects(); });
        connect(m_settings, &AppSettings::dynamicWidthChanged,
            this, [this] { updateGeometry(); });
        connect(m_settings, &AppSettings::pickerWidthChanged,
            this, [this] { updateGeometry(); });
        connect(m_settings, &AppSettings::maximumPickerWidthChanged,
            this, [this] { updateGeometry(); });
    }
    if (m_effectCapabilities) {
        connect(m_effectCapabilities, &EffectCapabilities::availabilityChanged,
            this, [this] { applyEffects(); });
    }
    connect(this, &QWindow::screenChanged, this, [this] { updateGeometry(); });
    const auto watchScreen = [this](QScreen *screen) {
        connect(screen, &QScreen::availableGeometryChanged,
            this, [this] { updateGeometry(); });
    };
    for (QScreen *screen : QGuiApplication::screens()) {
        watchScreen(screen);
    }
    connect(qGuiApp, &QGuiApplication::screenAdded, this, watchScreen);
    setResizeMode(QQuickView::SizeRootObjectToView);

    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint;
    if (m_mode != Mode::Demo) {
        flags |= Qt::WindowDoesNotAcceptFocus;
    }
    setFlags(flags);
    rootContext()->setContextProperty(QStringLiteral("completionController"), m_controller);
    rootContext()->setContextProperty(QStringLiteral("appSettings"), m_settings);
    rootContext()->setContextProperty(QStringLiteral("effectCapabilities"),
        m_effectCapabilities);
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

void PickerWindow::applyEffects()
{
    if (m_effectCapabilities) {
        m_effectCapabilities->refresh();
    }
    if (!handle()) {
        return;
    }

    QPainterPath shape;
    shape.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(size())), 8, 8);
    const QRegion region(shape.toFillPolygon().toPolygon());
    const bool blurAvailable = m_effectCapabilities && m_effectCapabilities->blurAvailable();
    const bool contrastAvailable = m_effectCapabilities
        && m_effectCapabilities->contrastAvailable();
    KWindowEffects::enableBlurBehind(this,
        m_settings && m_settings->blurEnabled() && blurAvailable, region);
    KWindowEffects::enableBackgroundContrast(this,
        m_settings && m_settings->contrastEnabled() && contrastAvailable,
        1.15, 1, 1, region);
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

void PickerWindow::setPositionedDirect(bool enabled)
{
    if (m_positionedDirect == enabled) {
        return;
    }
    m_positionedDirect = enabled;
    updateVisibility();
}

void PickerWindow::updateFallbackLayerPosition()
{
    if (!m_fallbackPositionValid || !m_fallbackShellIntegration) {
        return;
    }
    QScreen *screen = QGuiApplication::screenAt(m_fallbackGlobalPosition);
    const QRect screenGeometry = screen ? screen->geometry() : QRect();
    const QRect availableGeometry = screen ? screen->availableGeometry() : screenGeometry;
    QPoint localPosition = m_fallbackGlobalPosition - screenGeometry.topLeft();
    const QPoint availableOffset = availableGeometry.topLeft() - screenGeometry.topLeft();
    localPosition.setX(std::clamp(localPosition.x(), availableOffset.x(),
        std::max(availableOffset.x(), availableOffset.x() + availableGeometry.width() - width())));
    localPosition.setY(std::clamp(localPosition.y(), availableOffset.y(),
        std::max(availableOffset.y(), availableOffset.y() + availableGeometry.height() - height())));
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
        || (m_mode == Mode::InputPanel && !m_controller->isFallbackMode()
            && !m_positionedDirect)
        || (m_mode == Mode::Fallback
            && (m_controller->isFallbackMode() || m_positionedDirect)
            && m_fallbackPositionValid);
    if (m_controller->isVisible() && routeMatches) {
        if (m_mode == Mode::Demo) {
            const QPoint desiredPosition = QCursor::pos() + QPoint(16, 18);
            QScreen *cursorScreen = QGuiApplication::screenAt(QCursor::pos());
            const QRect available = cursorScreen
                ? cursorScreen->availableGeometry() : QRect(desiredPosition, size());
            setPosition(std::clamp(desiredPosition.x(), available.left(),
                            std::max(available.left(), available.right() - width() + 1)),
                std::clamp(desiredPosition.y(), available.top(),
                    std::max(available.top(), available.bottom() - height() + 1)));
        }
        show();
        applyEffects();
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
    if (m_mode == Mode::Demo) {
        targetScreen = QGuiApplication::screenAt(QCursor::pos());
    } else if (m_mode == Mode::Fallback && m_fallbackPositionValid) {
        targetScreen = QGuiApplication::screenAt(m_fallbackGlobalPosition);
    }
    const int availableHeight = targetScreen ? targetScreen->availableGeometry().height() : 480;
    const int availableWidth = targetScreen ? targetScreen->availableGeometry().width() : 1280;
    const int screenRows = std::max(1, (availableHeight - 80) / rowHeight);
    const int visibleRows = std::min({m_controller->candidates()->rowCount(),
        requestedRows, screenRows});
    int requestedWidth = m_settings
        ? m_settings->pickerWidth() : AppSettings::defaultPickerWidth;
    if (m_settings && m_settings->dynamicWidth()) {
        QFont aliasFont = QGuiApplication::font();
        aliasFont.setPixelSize(13);
        aliasFont.setWeight(QFont::DemiBold);
        const QFontMetrics metrics(aliasFont);
        int aliasWidth = 0;
        for (int row = 0; row < m_controller->candidates()->rowCount(); ++row) {
            const EmojiEntry *entry = m_controller->candidates()->entryAt(row);
            if (entry) {
                aliasWidth = std::max(aliasWidth, metrics.horizontalAdvance(
                    QStringLiteral(":%1:").arg(entry->alias)));
            }
        }
        constexpr int pickerChromeWidth = 74;
        const int contentWidth = std::max(220, pickerChromeWidth + aliasWidth);
        requestedWidth = std::min(contentWidth, m_settings->maximumPickerWidth());
    }
    const int pickerWidth = std::min(requestedWidth, std::max(1, availableWidth - 16));
    const QSize pickerSize(pickerWidth,
        verticalMargin + visibleRows * rowHeight);
    setMinimumSize(pickerSize);
    setMaximumSize(pickerSize);
    resize(pickerSize);
    if (isVisible()) {
        applyEffects();
    }
    if (m_mode == Mode::Fallback) {
        updateFallbackLayerPosition();
    }
}
