// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Rectangle {
    id: root

    color: palette.window
    border.color: Qt.alpha(palette.windowText, 0.20)
    border.width: 1
    radius: 8
    focus: true
    clip: true

    SystemPalette {
        id: palette
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Up) {
            completionController.moveSelection(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            completionController.moveSelection(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                   || event.key === Qt.Key_Tab) {
            completionController.select()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            completionController.dismiss()
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) {
            completionController.demoBackspace()
            event.accepted = true
        } else if (event.text.length > 0) {
            completionController.demoInput(event.text)
            event.accepted = true
        }
    }

    ListView {
        id: list

        anchors.fill: parent
        anchors.margins: 4
        model: completionController.candidates
        spacing: 0
        interactive: false
        currentIndex: completionController.candidates.selectedIndex

        delegate: Rectangle {
            id: row

            required property int index
            required property string emoji
            required property string alias
            required property bool selected

            width: list.width
            height: 38
            radius: 6
            color: selected ? palette.highlight : "transparent"

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Text {
                    width: 28
                    height: parent.height
                    text: row.emoji
                    color: row.selected ? palette.highlightedText : palette.windowText
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    height: parent.height
                    text: row.alias
                    color: row.selected ? palette.highlightedText : palette.windowText
                    font.pixelSize: 13
                    font.weight: row.selected ? Font.DemiBold : Font.Normal
                    verticalAlignment: Text.AlignVCenter
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: completionController.candidates.selectedIndex = row.index
                onClicked: completionController.select(row.index)
            }
        }
    }
}
