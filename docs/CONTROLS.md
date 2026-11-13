# Controls and accessibility

## Default desktop controls

| Action | Key |
| --- | --- |
| Move left / right | Left / Right, or A / D |
| Rotate clockwise | Up, or W |
| Rotate counterclockwise | Z |
| Soft drop | Down, or S |
| Hard drop | Space |
| Hold | C or Shift |
| Chat | Tab |
| Ready in room | R |
| Pause | F5, subject to room permissions |
| Target a player / use a special | 1–8 using the displayed player IDs |

Change gameplay bindings in **Settings → Controls**. The room host starts rounds from the room controls. Spectators can use arrows to switch the watched player.

## Make the game easier to read

Press **F2** to open or close Settings. Its Display tab includes window size, piece patterns and high contrast; Audio contains music and effects preferences. These preferences are saved locally.

- **F6** cycles 75%, 100%, 125% and 150% window sizes, bounded by the available display. You can also resize the window.
- **F7** toggles distinct piece markers, so piece types can be identified without relying on color alone.
- **F4** toggles higher-contrast text.

F2, F4, F6 and F7 are reserved for those options and cannot be assigned to gameplay actions. Website controls also have visible keyboard-focus outlines, and the background respects reduced-motion preferences.

## Xbox and PlayStation controllers

The desktop client uses SDL's controller mappings for supported Xbox and PlayStation pads over USB or Bluetooth. Enable **Controller input** in Settings → Display. Support depends on the operating system, connection and SDL mapping for the specific model. The terminal client does not use controllers.

| Xbox / PlayStation | In-game action |
| --- | --- |
| D-pad left/right or left stick | Move |
| D-pad down or left stick down | Soft drop |
| A / Cross | Rotate clockwise |
| B / Circle | Rotate counterclockwise |
| X / Square or LB / L1 | Hold |
| Y / Triangle or D-pad up | Hard drop |
| Start / Options | Pause, subject to room permissions |

Movement repeats while held. A stick dead zone prevents slight drift from moving pieces. Pads may connect or disconnect while the game runs. Input is ignored when the game loses focus or a dialog is open. Controllers follow the configured gameplay key bindings; keyboard and mouse remain available for menus, chat and special targeting. Full controller-only menu navigation is not provided.
