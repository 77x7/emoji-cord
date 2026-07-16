// SPDX-FileCopyrightText: 2026 Emoji-cord contributors
// SPDX-License-Identifier: GPL-3.0-or-later

function reportActiveWindow(window) {
    if (!window || !window.active) {
        callDBus("io.github.puzll.EmojiCord", "/Context",
            "io.github.puzll.EmojiCord.Context", "activeWindowChanged",
            "", "", "", "", false);
        return;
    }

    callDBus("io.github.puzll.EmojiCord", "/Context",
        "io.github.puzll.EmojiCord.Context", "activeWindowChanged",
        String(window.internalId), window.desktopFileName || "",
        window.resourceClass || "", window.resourceName || "", true);
}

let lastHeartbeat = 0;
function reportHeartbeat() {
    const now = Date.now();
    if (now - lastHeartbeat < 2000) {
        return;
    }
    lastHeartbeat = now;
    reportActiveWindow(workspace.activeWindow);
}

workspace.windowActivated.connect(reportActiveWindow);
workspace.cursorPosChanged.connect(reportHeartbeat);
reportActiveWindow(workspace.activeWindow);
