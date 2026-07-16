// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QQuickView>

#include <memory>

class CompletionController;
class InputPanelShellIntegration;
class WaylandInputMethod;

class PickerWindow final : public QQuickView
{
    Q_OBJECT

public:
    enum class Mode {
        Demo,
        InputPanel,
    };

    PickerWindow(CompletionController *controller, WaylandInputMethod *inputMethod,
        Mode mode, QWindow *parent = nullptr);
    ~PickerWindow() override;

    bool initialize(QString *error = nullptr);

private:
    void updateGeometry();

    CompletionController *m_controller = nullptr;
    WaylandInputMethod *m_inputMethod = nullptr;
    std::unique_ptr<InputPanelShellIntegration> m_shellIntegration;
    Mode m_mode = Mode::Demo;
};
