# Apple TV (Preview)

The tvOS client shares the game, soundtrack and network protocol, with its own widescreen interface. Large buttons, directional focus, a dedicated room screen and phone/PIN sign-in are designed for use from the couch without a keyboard. It supports Apple remotes and SDL-compatible Xbox/PlayStation gamepads. This is a preview: physical Apple TV controller, sleep/resume and installation testing are still required. It is not an App Store release.

## Build

Use a Mac with Xcode, the tvOS SDK and CMake (`brew install cmake`). Select Xcode's developer tools if command-line tools point elsewhere:

```sh
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
bash scripts/build-tvos.sh simulator
bash scripts/build-tvos.sh device
```

SDL2 is reused from `~/Development/tools/tvos/sdk/{simulator,device}`. If absent, the script downloads the pinned SDL2 source, verifies its SHA256 and builds it into that shared tools directory. Override the tools location with `TVOS_TOOLS`.

Outputs:

- Simulator: `dist/tvos/simulator/build/LineClash.app` (ad-hoc signed).
- Device: `dist/tvos/device/build/LineClash.app` (unsigned).
- Xcode device project: `dist/tvos/device/xcode/LineClash.xcodeproj`.

Open the device project, choose your Apple Development signing team, pair your Apple TV in Xcode and build/run it. A macOS Developer ID certificate cannot provision a tvOS app. No signing credentials are changed by the build script.

For the simulator, select an Apple TV device in Xcode's Simulator or Device Hub. Use its identifier from `xcrun simctl list devices available`:

```sh
xcrun simctl boot DEVICE_ID
xcrun simctl bootstatus DEVICE_ID -b
xcrun simctl install DEVICE_ID dist/tvos/simulator/build/LineClash.app
xcrun simctl launch DEVICE_ID se.kladhest.lineclash.tv
```

## Apple remote

| Control | Menus | During a round |
| --- | --- | --- |
| Left / Right (click or swipe) | Move focus | Move piece (hold to repeat) |
| Down | Move focus | Soft drop (hold to repeat) |
| Up | Move focus | Hard drop |
| Click / Select | Select focused item | Rotate clockwise |
| Play/Pause | — | Hold piece |
| Back / Menu | Previous screen / close overlay | Game action menu |

The game menu provides target selection, use-special, pause/resume, settings and return to the lobby. The round continues while this local menu is open; pausing a round still follows the server's permissions. The TV/Home button belongs to tvOS. Use it to leave the application.

## Gamepads

Pair an Xbox, PlayStation or another supported extended gamepad in Apple TV Settings. The client accepts hot-plugged controllers.

| Control | Action |
| --- | --- |
| D-pad / left stick | Menu navigation; left/right/down move/drop in play |
| A / Cross | Select in menus; rotate clockwise in play |
| B / Circle | Back in menus; rotate counterclockwise in play |
| X / Square or left shoulder | Hold |
| Y / Triangle or D-pad Up | Hard drop |
| Right shoulder | Next special target |
| Right stick click | Use selected special |
| Start / Options | Pause/resume, subject to server permissions |
| Back / Share | Game action menu |

The Apple remote remains available alongside the gamepad. In text dialogs, tvOS handles keyboard navigation; no hardware keyboard is required.

## Accounts and servers

Use **Account → Sign in with a PIN**, then enter the displayed PIN at `https://lineclash.kladhest.se/pin/` on a phone or computer. Email/password sign-in and account creation also use the native TV keyboard. Account and directory requests use Foundation HTTPS with normal certificate validation and bounded responses. Redirects are refused. Passwords and account tokens remain in memory.

Public and saved servers use the existing TCP game protocol. Gameplay is not encrypted; account passwords are sent only to the central HTTPS endpoint. Settings, saved server entries and the remembered account name are stored in the app's Caches directory, which tvOS may purge. Re-enter them if storage is reclaimed.

For a private LAN server, add its IP and TCP port in **Saved servers**. Local network scanning uses IPv4 broadcast; physical-device broadcast discovery remains unverified. The app includes a local-network permission description, but does not claim a multicast entitlement. Public discovery and manual server addresses do not depend on broadcast scanning.

## Checks

```sh
make tvos-focus-test
make tvos-ui-test
make -C tests gui discovery
```

Both simulator and device builds must compile before updating the preview. Test navigation, native text entry, PIN sign-in, joining a room, ready/start, held movement, specials, music and returning from background on the target Apple TV before distributing a signed build.
