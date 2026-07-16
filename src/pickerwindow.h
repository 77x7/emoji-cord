// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QQuickView>

#include <memory>

class CompletionController;
class FallbackLayerShellIntegration;
class InputPanelShellIntegration;
class WaylandInputMethod;

class PickerWindow final : public QQuickView
{
    Q_OBJECT

public:
    enum class Mode {
        Demo,
        InputPanel,
        Fallback,
    };

    PickerWindow(CompletionController *controller, WaylandInputMethod *inputMethod,
        Mode mode, QWindow *parent = nullptr);
    ~PickerWindow() override;

    bool initialize(QString *error = nullptr);
    void setFallbackPosition(const QPoint &globalPosition);
    void clearFallbackPosition();

private:
    void updateGeometry();
    void updateVisibility();
    void updateFallbackLayerPosition();

    CompletionController *m_controller = nullptr;
    WaylandInputMethod *m_inputMethod = nullptr;
    std::unique_ptr<InputPanelShellIntegration> m_shellIntegration;
    std::unique_ptr<FallbackLayerShellIntegration> m_fallbackShellIntegration;
    Mode m_mode = Mode::Demo;
    QPoint m_fallbackGlobalPosition;
    bool m_fallbackPositionValid = false;
};
