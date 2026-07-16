// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string inputError: ""

    width: 520
    height: 300
    color: palette.window

    SystemPalette {
        id: palette
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
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

        Item { Layout.fillHeight: true }
    }
}
