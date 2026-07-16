// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pickerwindow.h"

#include "completioncontroller.h"
#include "inputpanelshell.h"
#include "waylandinputmethod.h"

#include <QCursor>
#include <QQmlContext>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

PickerWindow::PickerWindow(CompletionController *controller,
    WaylandInputMethod *inputMethod, Mode mode, QWindow *parent)
    : QQuickView(parent)
    , m_controller(controller)
    , m_inputMethod(inputMethod)
    , m_mode(mode)
{
    setTitle(QStringLiteral("Emoji-cord"));
    setColor(Qt::transparent);
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
        if (m_controller->isVisible()) {
            if (m_mode != Mode::InputPanel) {
                setPosition(QCursor::pos() + QPoint(16, 18));
            }
            show();
            if (m_mode == Mode::Demo) {
                requestActivate();
            }
        } else {
            hide();
        }
    });
    connect(m_controller->candidates(), &QAbstractItemModel::modelReset,
        this, &PickerWindow::updateGeometry);
}

PickerWindow::~PickerWindow() = default;

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
    if (m_mode != Mode::InputPanel) {
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

    m_shellIntegration = std::make_unique<InputPanelShellIntegration>(m_inputMethod);
    if (!m_shellIntegration->initialize(waylandWindow->display())) {
        if (error) {
            *error = QStringLiteral("KWin input-panel protocol is unavailable");
        }
        return false;
    }
    waylandWindow->setShellIntegration(m_shellIntegration.get());
    if (error) {
        error->clear();
    }
    return true;
}

void PickerWindow::updateGeometry()
{
    constexpr int rowHeight = 38;
    constexpr int verticalMargin = 8;
    const QSize pickerSize(280,
        verticalMargin + m_controller->candidates()->rowCount() * rowHeight);
    setMinimumSize(pickerSize);
    setMaximumSize(pickerSize);
    resize(pickerSize);
}
