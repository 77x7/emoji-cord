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
        anchors.rightMargin: contentHeight > height ? 14 : 4
        model: completionController.candidates
        spacing: 0
        interactive: contentHeight > height
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

    Item {
        id: scrollBar

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        anchors.rightMargin: 3
        width: 10
        visible: list.contentHeight > list.height

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 4
            height: parent.height
            radius: 2
            color: Qt.alpha(palette.windowText, 0.14)
        }

        Rectangle {
            id: scrollThumb

            anchors.horizontalCenter: parent.horizontalCenter
            width: 4
            height: Math.max(20, scrollBar.height * list.height / list.contentHeight)
            y: {
                const range = list.contentHeight - list.height
                const progress = range > 0
                    ? Math.max(0, Math.min(1, list.contentY / range))
                    : 0
                return progress * (scrollBar.height - height)
            }
            radius: 2
            color: palette.highlight
        }

        MouseArea {
            anchors.fill: parent

            function moveTo(pointerY) {
                const thumbRange = scrollBar.height - scrollThumb.height
                if (thumbRange <= 0) {
                    return
                }
                const progress = Math.max(0, Math.min(1,
                    (pointerY - scrollThumb.height / 2) / thumbRange))
                list.contentY = progress * (list.contentHeight - list.height)
            }

            onPressed: mouse => moveTo(mouse.y)
            onPositionChanged: mouse => {
                if (pressed) {
                    moveTo(mouse.y)
                }
            }
        }
    }
}
