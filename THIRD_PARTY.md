# Third-party components

Emoji-cord is an independent implementation. It uses system libraries and
standard protocols but does not incorporate Plasma Keyboard, Fcitx, Espanso,
or Discord source code or assets.

## Wayland input-method protocol

`protocols/input-method-unstable-v1.xml` is a compact protocol description
preserving the wire interface defined by Wayland's historical
`input-method-unstable-v1` protocol.

- Copyright 2012-2013 Intel Corporation
- License: MIT
- Source: https://gitlab.freedesktop.org/wayland/wayland-protocols

## Development emoji catalog

`data/emoji-demo.json` is a small transformed development subset of gemoji.

- Copyright 2019 GitHub, Inc.
- License: MIT
- Source: https://github.com/github/gemoji

The complete release vocabulary will be generated from pinned gemoji and
Unicode/CLDR sources and will include a machine-generated provenance report.

## Layer-shell protocol

`protocols/wlr-layer-shell-unstable-v1.xml` is a compact wire-compatible
description of the wlr layer-shell protocol used by the separate Steam fallback
picker.

- Copyright 2017 Drew DeVault
- License: MIT
- Source: https://gitlab.freedesktop.org/wlroots/wlr-protocols

## Runtime and build dependencies

Emoji-cord dynamically links Qt 6, Qt Wayland Client, Wayland Client,
xkbcommon, Xlib, XI, and XTest from the operating system. Their source is not
bundled. The
caret-positioned picker uses QtWayland private interfaces and must be rebuilt
when the distribution changes to an ABI-incompatible Qt build.

KDE Plasma Keyboard 6.7 demonstrated the feasibility of using an input-method
keyboard grab together with the Wayland input-panel overlay role. Emoji-cord's
controller, popup, protocol client, and shell integration are separately
implemented for this project.
