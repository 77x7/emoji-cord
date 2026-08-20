# Emoji-cord

Emoji-cord is a system-wide, colon-triggered emoji autocomplete application for
KDE Plasma on Wayland. It aims to provide the interaction people expect from
chat applications everywhere on the desktop:

```text
:sk
  💀  :skull:
  ☠️  :skull_and_crossbones:
  ⛷️  :skier:
```

The picker is a compact vertical list placed next to the text cursor. Results
are matched by familiar aliases and ranked using local usage frequency. It is
designed to work in regular Qt, GTK, Electron, and terminal applications as
well as difficult targets such as Steam chat and Plasma's own search fields.

Emoji-cord is an independent project. It does not embed, extend, or load code
from Fcitx, Plasma Keyboard, Discord, or Espanso.

## Status

Emoji-cord is in early development and is not ready for general use. The
catalog, query, matching, usage, Wayland input-method connection, and vertical
caret-positioned picker are implemented. On the current development machine,
the app imports the existing local 1,827-entry Fcitx TSV vocabulary. The
bundled catalog remains a small licensed fixture for clean builds. Evdev,
uinput, and normal-clipboard fallback experiments were removed after proving
too disruptive. Steam client build `1782866176` is supported through a separate
XWayland compatibility backend because its CEF helper creates no focused IBus
or XIM context. Steam replacement and keyboard navigation work. Its separate
layer-shell picker stays near the recorded text-field click because Steam
exposes no caret rectangle.

## Building the development scaffold

Install the Fedora/Nobara build dependencies:

```bash
sudo dnf install cmake ninja-build gcc-c++ \
  qt6-qtbase-devel qt6-qtbase-private-devel \
  qt6-qtdeclarative-devel qt6-qtwayland-devel \
  kf6-kwindowsystem-devel \
  wayland-devel libxkbcommon-devel libX11-devel libXi-devel libXtst-devel
```

Configure, build, and test:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

### Safe picker preview

The demo does not replace or communicate with the active input method:

```bash
./build/emoji-cord --demo-picker
```

It opens the vertical picker near the pointer with a preview query of `sk`.
While the demo window is focused, typing refines the query, Backspace removes
characters, and typing `:` starts a fresh query. Hovering selects a row; use a
mouse click, Up/Down, Enter, or Escape to exercise the interface. Selecting or
dismissing exits demo mode because it is not attached to a target text field.

### End-to-end Plasma test

Install the development build for the current user:

```bash
cmake --install build --prefix "$HOME/.local"
update-desktop-database "$HOME/.local/share/applications"
kwriteconfig6 --file kwinrc --group Plugins \
  --key emoji-cord-contextEnabled true
```

Open **System Settings > Keyboard > Virtual Keyboard**, select **Emoji-cord**,
and test in KWrite or another normal Qt text field. When present, Emoji-cord
loads `~/.local/share/fcitx5/emojicomplete/emoji.tsv`, currently containing
1,827 aliases.

The installed KWin script reports active-window identity for the Steam
compatibility path. Log out and back in after enabling it for the first time.

Expected behavior:

- `:sk` opens a vertical picker at the caret.
- Up/Down moves the highlighted row.
- Enter or Tab replaces the shortcode with the selected emoji.
- A complete `:skull:` replaces itself immediately.
- Escape dismisses the picker without changing the typed shortcode.

### Settings

Open **Emoji-cord Settings** from the KDE Application Launcher, or run:

```bash
emoji-cord --settings
```

**Suggestions shown at once** accepts any positive integer and defaults to 8.
The value controls the picker's viewport height; every matching suggestion is
still loaded and remains available by scrolling. It is stored in
`~/.config/emoji-cord/settings.json` and is applied immediately when the input
method is running. The viewport is constrained further when necessary to fit
the current screen.

The appearance section controls picker background opacity, KWin blur-behind,
and KWin background contrast. Automatic opacity uses 85% when KWin's blur
effect is available and enabled for the picker, otherwise 100%. Explicit
opacity values range from 20% to 100%. Blur and contrast are requested through
KWindowEffects, so KWin remains responsible for rendering them and its global
effect availability is respected. These settings apply to both native Wayland
and Steam fallback pickers, not to the settings window.

Picker width defaults to the original fixed 280 pixels. **Automatic** mode
measures the matching aliases and grows only as far as the configured maximum
width. Fixed and maximum widths accept values from 220 to 2,000 pixels, with
the final popup always constrained to the active screen.

Switch back to **Fcitx 5** in the same settings page after testing. If keyboard
input is unusable, switch to a TTY and restore the configured input method:

```bash
kwriteconfig6 --file kwinrc --group Wayland --key InputMethod \
  /usr/share/applications/org.fcitx.Fcitx5.desktop
```

Then log out and back in.

## Goals

- Open a vertical Discord-style autocomplete list after `:` and a matching
  query.
- Replace the typed shortcode with the selected Unicode emoji.
- Work across as much of a Plasma Wayland session as technically possible.
- Prefer direct input-method commits and use only compositor-authorized
  fallback control when an application lacks a usable text-input context.
- Place the popup at the caret when the target exposes reliable geometry.
- Follow the active KDE color scheme, font, scale, and accent color.
- Rank frequently selected emoji above less frequently selected alternatives.
- Keep all usage data on the local machine.
- Ship one application executable with no Fcitx or Espanso runtime dependency.
- Use only redistributable data and dependencies with complete attribution.

## Non-goals

- Reimplementing Plasma Keyboard's press-and-hold diacritic selector.
- Providing CJK or other general-purpose input methods.
- Copying Discord's assets, branding, or exact visual design.
- Bundling an emoji image set. Emoji-cord renders glyphs from the user's
  configured system fonts.
- Recording general typing history.

## Interaction

Typing a colon arms completion. ASCII letters, digits, `_`, `+`, and `-` extend
the query. The popup stays hidden until at least one result exists.

| Input | Action |
| --- | --- |
| `Down` / `Up` | Move through candidates |
| `Enter` / `Tab` | Select the highlighted candidate |
| `:exact_alias:` | Select an exact alias |
| `Escape` | Dismiss completion without changing text |
| `Backspace` | Shorten the query and update results |
| Mouse click | Select a candidate |

The popup loads every matching suggestion and shows eight rows at once by
default. Each row contains an emoji preview and its canonical `:alias:`. The
selected row uses the current KDE selection and accent colors. Additional
results remain available by scrolling, and the window does not take keyboard
focus from the target application.

KRunner advertises keyboard repeat as disabled to input methods while handling
held arrow keys through its own result list. Emoji-cord therefore supports
discrete Up/Down presses in KRunner, but held-arrow navigation is not supported
there. Normal text fields use the compositor's repeat rate.

## Matching and ranking

Search is deterministic and divided into quality tiers:

1. Exact alias.
2. Alias prefix.
3. Token or substring match.
4. Fuzzy match for small omissions, transpositions, or typing errors.

Within a quality tier, results are ordered by descending selection count.
The most recent selection breaks equal-frequency ties, followed by the
canonical alias for stable ordering. A strong exact match always remains above
a frequently used but weak fuzzy match.

Usage records contain only the canonical alias, selection count, and last-use
sequence. They are stored under the XDG data directory, normally
`~/.local/share/emoji-cord/usage.json`, and written atomically.

## Architecture

Emoji-cord is a single C++20 and Qt 6 executable. Installation also provides
ordinary package resources such as its desktop entry, vocabulary, license
texts, and device-access rules.

### Supported text-input contexts

KDE launches the selected virtual keyboard on a privileged Wayland connection.
Emoji-cord registers as a Plasma virtual keyboard and implements the unstable
Wayland input-method v1 and input-panel v1 protocols used by KWin.

For applications that expose a text-input context, Emoji-cord will:

1. Receive physical keyboard events through the input-method keyboard grab.
2. Forward every event it does not consume to the focused application.
3. Track a colon query without allowing the popup to steal focus.
4. Create an input-panel surface with the overlay-panel role.
5. Let KWin position and constrain that surface relative to the application's
   caret rectangle.
6. Delete the shortcode and commit the selected Unicode string directly.

KWin owns caret placement in this path. It accounts for output geometry,
fractional scaling, window movement, and whether the popup fits above or below
the cursor.

This is an independent implementation of public Wayland protocols. Plasma
Keyboard demonstrates that the protocol flow works, but its selector,
controllers, QML, data, and assets are not part of Emoji-cord.

### Unsupported contexts

Steam and some shell or custom-rendered fields do not expose a usable Wayland
text-input context. A normal Wayland client cannot globally observe keyboard
input, discover the caret, or inject text into these applications.

Steam under XWayland uses a separate compatibility backend. It passively reads
XI2 events without grabbing or forwarding normal typing, retains only the
active shortcode, and rechecks the active X11 window on every event. Direct
Wayland input-method contexts always take priority, and fallback eligibility is
limited to an explicit Steam identity allowlist.

Replacement sends native Backspaces through XTEST, temporarily offers the emoji
through X11's PRIMARY selection, and middle-clicks the saved Steam text-field
position. Existing PRIMARY text is copied and re-offered after insertion. While
the fallback candidate list is visible, only Up, Down, Enter, keypad Enter, and
Escape are grabbed so Steam cannot act on navigation keys. The whole keyboard
is never grabbed.

Steam does not expose a caret rectangle through text-input, XIM, or AT-SPI on
the tested build. The fallback picker therefore uses a separate layer-shell
surface at the recorded text-field click and does not pretend to track the
caret. The normal Wayland picker remains compositor-positioned at the real
caret and is not modified by this compatibility backend.

### Active application and suppression

An optional KWin script reports the active application's desktop-file name and
XWayland resource identity to Emoji-cord. These values are routing hints, not
security identities, and are matched only against an explicit allowlist.

Completion is suspended when:

- The screen is locked.
- A supported text context reports password or sensitive content.
- The active application is not the explicitly supported Steam XWayland client.

Unsupported applications cannot reveal whether an individual field is a
password field. The current Steam-wide fallback cannot distinguish chat from
login fields, so it must not be used while entering sensitive text. It can be
disabled with `--disable-xwayland-fallback`.

### Runtime components

- `QueryState`: colon arming, bounded query editing, cancellation, and exact
  closing-colon behavior.
- `EmojiCatalog`: immutable alias, emoji, and search-key records.
- `EmojiMatcher`: quality tiers, fuzzy scoring, and deterministic ordering.
- `UsageStore`: atomic local frequency and recency persistence.
- `InputMethodBackend`: privileged Wayland input method and direct commits.
- `ContextRouter`: active application, direct-context priority, and fail-closed
  fallback eligibility.
- `XWaylandFallback`: Steam-only XI2 observation, scoped navigation grabs,
  XTEST deletion, and PRIMARY insertion.
- `PickerWindow`: themed, vertical, non-activating candidate overlay.

## Why Emoji-cord is a virtual keyboard

Wayland deliberately prevents ordinary applications from reading arbitrary
keystrokes and placing windows at another application's caret. Registering as
the selected virtual keyboard provides the compositor-mediated input-method
connection needed for correct behavior in cooperating applications.

KWin permits one selected virtual keyboard/input method at a time. Selecting
Emoji-cord therefore replaces Fcitx or Plasma Keyboard in that role. The
fallback exists because even a selected input method cannot receive a text
context from applications that do not implement the relevant protocols.

## Security and privacy

Fallback input control is powerful and must be visible to users and
distributors. Emoji-cord follows these constraints:

- No raw input-device access, whole-keyboard grab, uinput device, or
  continuously running privileged helper is used.
- XI2 events outside the verified Steam route are discarded immediately.
- Only fallback navigation keys are grabbed, and only while candidates are
  visible.
- Unrelated text is not buffered, logged, or persisted.
- Query storage is capped and cleared on focus changes, cancellation, lock, and
  unsupported characters.
- Sensitive input suppression is used whenever the target provides metadata.
- Fallback can be disabled globally from the command line.
- Usage history contains emoji aliases only and can be cleared from settings.

The project will include a dedicated threat-model document before its first
release.

## Vocabulary and provenance

The vocabulary previously installed from the Espanso package
`actually-all-emojis` will not be included. Its upstream repository does not
declare a license, so public availability alone does not permit
redistribution.

Emoji-cord instead generates its catalog from clearly licensed sources:

- [github/gemoji](https://github.com/github/gemoji), MIT License, for familiar
  aliases.
- [Unicode emoji and CLDR data](https://unicode.org/), Unicode License v3, for
  current emoji sequences, standardized names, and search keywords.

The generator is reproducible and records source revisions, URLs, and hashes.
Generated records use gemoji's first alias as the canonical shortcode, retain
additional aliases as search keys, and add Unicode names and keywords without
silently replacing familiar aliases. Alias collisions are resolved by a
documented deterministic rule and covered by tests.

Network access is never needed at application runtime.

## Licensing

Emoji-cord is licensed under `GPL-3.0-or-later`.

The repository uses SPDX identifiers and follows the REUSE specification.
Third-party data keeps its own license and attribution. Release archives will
contain:

- The GPL-3.0-or-later license text.
- The MIT notice for gemoji and any other MIT-licensed source incorporated in
  generated artifacts.
- Unicode License v3 and the required Unicode copyright notice.
- A generated third-party attribution and provenance report.
- Source code for every generator used to create bundled data.

Qt, Wayland, xkbcommon, Xlib, XI, and XTest are dynamically linked system
dependencies. Their notices and exact
roles will be documented in `THIRD_PARTY.md`. Emoji-cord is not affiliated
with or endorsed by Discord, GitHub, KDE, or Unicode, Inc.

## Planned dependencies

- CMake 3.24 or newer
- A C++20 compiler
- Qt 6 Core, Gui, Quick, QML, DBus, and Test
- Qt Wayland development interfaces
- Wayland client and scanner
- xkbcommon
- Xlib, XI2, and XTest for the Steam XWayland compatibility backend

Fedora and Nobara package names will be documented once the first end-to-end
prototype builds successfully.

## Development roadmap

### Milestone 1: Core

- CMake and Qt test scaffold.
- Catalog data model and loader.
- Colon-query state machine.
- Exact, prefix, substring, and fuzzy matching.
- Frequency and recency ranking.
- Atomic usage persistence.
- Unit tests for Unicode sequences, matching, ranking, and cancellation.

### Milestone 2: Wayland input method

- Plasma virtual-keyboard registration.
- Generated input-method v1 and input-panel v1 protocol bindings.
- Keyboard grab and lossless forwarding of unhandled events.
- Direct surrounding-text deletion and Unicode commit.
- Content-purpose and sensitive-field handling.
- Lifecycle recovery when KWin restarts or reopens the connection.

### Milestone 3: Picker

- Vertical Qt Quick candidate list.
- KDE theme, font, accent, and scale integration.
- Compositor-positioned overlay-panel surface.
- Keyboard and mouse selection.
- Correct geometry on multiple mixed-scale outputs.
- Accessible names and high-contrast behavior.

### Milestone 4: Unsupported-application fallback

- Steam-only XI2 observation and XTEST deletion.
- Scoped fallback navigation-key grabs.
- PRIMARY selection preservation and Unicode insertion.
- Active-application routing and lock-screen suspension.
- A reliable source for Steam caret geometry.

### Milestone 5: Distribution

- Settings and pause control.
- RPM packaging for Fedora and Nobara.
- Reproducible vocabulary update workflow.
- REUSE compliance and third-party report.
- Threat model, privacy documentation, and security review.
- Translation infrastructure and contributor documentation.

## Test matrix

Automated tests cover query transitions, matching tiers, fuzzy score stability,
usage ordering, malformed data, atomic persistence, multi-codepoint emoji,
skin-tone sequences, flags, key forwarding, and fallback route isolation.

Manual release testing includes:

- Plasma Application Launcher and KRunner.
- Steam chat and Steam login fields.
- Native Qt and GTK text fields.
- Chromium/Electron applications.
- Native Wayland and XWayland clients.
- Terminals with application-specific paste shortcuts.
- Password fields, unsupported applications, and the lock screen.
- Multiple keyboards, keyboard reconnects, and layout changes.
- Multiple monitors with fractional and mixed scaling.
- PRIMARY preservation and focus-change races.

## Acceptance criteria for the first usable release

- Typing `:sk` opens one vertical, KDE-themed picker without moving focus.
- Selecting `:skull:` replaces the typed shortcode with `💀`.
- Selection works through direct commits in compliant clients.
- The same interaction works through fallback insertion in Steam chat and
  direct commits in the Plasma Application Launcher.
- Frequently selected aliases are ranked first among otherwise equivalent
  matches and survive application restarts.
- No raw query or unrelated typed text is written to logs or usage storage.
- Locking the session immediately hides the popup and clears transient state.
- The release contains complete source, data provenance, and license notices.

## Contributing

The contribution workflow will be added after the initial scaffold stabilizes.
All contributions must include an SPDX copyright and license declaration and
must not introduce data or assets without a redistributable license and clear
provenance.
