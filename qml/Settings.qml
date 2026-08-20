// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string inputError: ""
    readonly property int automaticOpacity: effectCapabilities.blurAvailable
        && appSettings.blurEnabled
        ? 85 : 100

    width: 680
    height: 580
    color: palette.window

    SystemPalette {
        id: palette
    }

    ScrollView {
        id: page

        anchors.fill: parent
        anchors.margins: 24
        clip: true

        ColumnLayout {
            width: page.availableWidth
            spacing: 24

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: qsTr("Emoji-cord Settings")
                color: palette.windowText
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Tune the completion picker. Changes are saved and applied immediately.")
                color: Qt.alpha(palette.windowText, 0.70)
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: settingRow.implicitHeight + 32
            radius: 10
            color: Qt.alpha(palette.windowText, 0.055)
            border.color: Qt.alpha(palette.windowText, 0.12)

            RowLayout {
                id: settingRow

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 16
                spacing: 20

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: qsTr("Suggestions shown at once")
                        color: palette.windowText
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Sets the picker height. All matching suggestions remain scrollable.")
                        color: Qt.alpha(palette.windowText, 0.65)
                        wrapMode: Text.WordWrap
                    }
                }

                RowLayout {
                    spacing: 6

                    ToolButton {
                        text: "−"
                        enabled: Number(maximumField.text) > 1
                        onClicked: {
                            maximumField.text = String(Number(maximumField.text) - 1)
                            maximumField.applyValue()
                        }
                    }

                    TextField {
                        id: maximumField

                        Layout.preferredWidth: 112
                        text: String(appSettings.visibleSuggestions)
                        horizontalAlignment: Text.AlignHCenter
                        inputMethodHints: Qt.ImhDigitsOnly
                        selectByMouse: true
                        validator: IntValidator { bottom: 1; top: 2147483647 }

                        function applyValue() {
                            if (acceptableInput) {
                                root.inputError = ""
                                appSettings.updateVisibleSuggestions(Number(text))
                            } else {
                                root.inputError = qsTr("Enter a positive whole number.")
                            }
                        }

                        onTextEdited: root.inputError = ""
                        onEditingFinished: applyValue()
                        Keys.onReturnPressed: event => {
                            applyValue()
                            event.accepted = true
                        }
                        Keys.onEnterPressed: event => {
                            applyValue()
                            event.accepted = true
                        }

                        Connections {
                            target: appSettings
                            function onVisibleSuggestionsChanged(value) {
                                if (!maximumField.activeFocus) {
                                    maximumField.text = String(value)
                                }
                            }
                        }
                    }

                    ToolButton {
                        text: "+"
                        enabled: Number(maximumField.text) < 2147483647
                        onClicked: {
                            maximumField.text = String(Number(maximumField.text) + 1)
                            maximumField.applyValue()
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: appearanceLayout.implicitHeight + 32
            radius: 10
            color: Qt.alpha(palette.windowText, 0.055)
            border.color: Qt.alpha(palette.windowText, 0.12)

            ColumnLayout {
                id: appearanceLayout

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 16
                spacing: 12

                Label {
                    text: qsTr("Appearance")
                    color: palette.windowText
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Background opacity")
                        color: palette.windowText
                    }

                    CheckBox {
                        id: automaticOpacity
                        text: qsTr("Automatic")
                        checked: appSettings.backgroundOpacity === 0
                        onClicked: appSettings.updateBackgroundOpacity(checked
                            ? 0 : root.automaticOpacity)
                    }

                    Slider {
                        id: opacitySlider

                        Layout.preferredWidth: 170
                        from: 20
                        to: 100
                        stepSize: 1
                        enabled: !automaticOpacity.checked
                        value: appSettings.backgroundOpacity === 0
                            ? root.automaticOpacity : appSettings.backgroundOpacity
                        onValueChanged: {
                            if (enabled && activeFocus) {
                                opacitySaveTimer.restart()
                            }
                        }

                        Timer {
                            id: opacitySaveTimer

                            interval: 200
                            onTriggered: appSettings.updateBackgroundOpacity(
                                Math.round(opacitySlider.value))
                        }
                    }

                    Label {
                        Layout.preferredWidth: 42
                        text: Math.round(opacitySlider.value) + "%"
                        horizontalAlignment: Text.AlignRight
                        color: palette.windowText
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: qsTr("Blur behind picker")
                            color: palette.windowText
                        }

                        Label {
                            text: effectCapabilities.blurAvailable
                                ? qsTr("Rendered by KWin")
                                : qsTr("Unavailable in the current KWin effects")
                            color: Qt.alpha(palette.windowText, 0.60)
                            font.pixelSize: 12
                        }
                    }

                    Switch {
                        checked: appSettings.blurEnabled
                        enabled: effectCapabilities.blurAvailable
                        onClicked: appSettings.updateBlurEnabled(checked)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: qsTr("Background contrast")
                            color: palette.windowText
                        }

                        Label {
                            text: effectCapabilities.contrastAvailable
                                ? qsTr("Uses KWin's readability effect")
                                : qsTr("Unavailable in the current KWin effects")
                            color: Qt.alpha(palette.windowText, 0.60)
                            font.pixelSize: 12
                        }
                    }

                    Switch {
                        checked: appSettings.contrastEnabled
                        enabled: effectCapabilities.contrastAvailable
                        onClicked: appSettings.updateContrastEnabled(checked)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: widthLayout.implicitHeight + 32
            radius: 10
            color: Qt.alpha(palette.windowText, 0.055)
            border.color: Qt.alpha(palette.windowText, 0.12)

            ColumnLayout {
                id: widthLayout

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 16
                spacing: 12

                Label {
                    text: qsTr("Picker width")
                    color: palette.windowText
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Sizing mode")
                        color: palette.windowText
                    }

                    ComboBox {
                        id: widthMode

                        Layout.preferredWidth: 180
                        model: [qsTr("Fixed"), qsTr("Automatic")]
                        currentIndex: appSettings.dynamicWidth ? 1 : 0
                        onActivated: index => {
                            if (!appSettings.updateDynamicWidth(index === 1)) {
                                currentIndex = appSettings.dynamicWidth ? 1 : 0
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: appSettings.dynamicWidth
                                ? qsTr("Maximum width") : qsTr("Fixed width")
                            color: palette.windowText
                        }

                        Label {
                            text: appSettings.dynamicWidth
                                ? qsTr("Fits the longest alias without exceeding this value.")
                                : qsTr("Uses this width for every suggestion list.")
                            color: Qt.alpha(palette.windowText, 0.60)
                            font.pixelSize: 12
                        }
                    }

                    TextField {
                        id: widthValue

                        Layout.preferredWidth: 130
                        text: String(appSettings.dynamicWidth
                            ? appSettings.maximumPickerWidth : appSettings.pickerWidth)
                        horizontalAlignment: Text.AlignHCenter
                        inputMethodHints: Qt.ImhDigitsOnly
                        selectByMouse: true
                        validator: IntValidator { bottom: 220; top: 2000 }

                        function applyValue() {
                            if (!acceptableInput) {
                                text = String(appSettings.dynamicWidth
                                    ? appSettings.maximumPickerWidth : appSettings.pickerWidth)
                                return
                            }
                            const number = Number(text)
                            const accepted = appSettings.dynamicWidth
                                ? appSettings.updateMaximumPickerWidth(number)
                                : appSettings.updatePickerWidth(number)
                            if (!accepted) {
                                text = String(appSettings.dynamicWidth
                                    ? appSettings.maximumPickerWidth : appSettings.pickerWidth)
                            }
                        }

                        onEditingFinished: applyValue()
                        Keys.onReturnPressed: event => {
                            applyValue()
                            event.accepted = true
                        }
                        Keys.onEnterPressed: event => {
                            applyValue()
                            event.accepted = true
                        }
                    }

                    Label {
                        text: "px"
                        color: palette.windowText
                    }
                }

                Connections {
                    target: appSettings

                    function onDynamicWidthChanged(enabled) {
                        widthMode.currentIndex = enabled ? 1 : 0
                        widthValue.text = String(enabled
                            ? appSettings.maximumPickerWidth : appSettings.pickerWidth)
                    }
                    function onPickerWidthChanged(value) {
                        if (!appSettings.dynamicWidth && !widthValue.activeFocus) {
                            widthValue.text = String(value)
                        }
                    }
                    function onMaximumPickerWidthChanged(value) {
                        if (appSettings.dynamicWidth && !widthValue.activeFocus) {
                            widthValue.text = String(value)
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.inputError.length > 0
                ? root.inputError
                : appSettings.error.length > 0
                ? appSettings.error
                : appSettings.status.length > 0
                ? appSettings.status
                : qsTr("Current value: %1").arg(appSettings.visibleSuggestions)
            color: root.inputError.length > 0 || appSettings.error.length > 0
                ? palette.highlight
                : Qt.alpha(palette.windowText, 0.62)
        }

        }
    }
}
