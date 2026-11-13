# Playing LineClash

Start the server, then open `./clients/gui/lineclash`. In the GUI, add/select the host and port, enter a nickname and password, and connect. A new nickname creates an account unless registration is disabled. Reuse the same credentials to log in later. Choose a room from the lobby list.

The first player in a room becomes host. Ready players can start a round using the room's start control; the host can manage bots and room rules. Players on the same named team cooperate. Set or clear the team in the lobby. Room and server chat are separate; the GUI exposes both and room settings through its controls. Spectators watch without playing; use the GUI room controls to spectate and arrows to change the watched player. An eliminated player can also watch opponents.

## Default desktop controls

| Action | Desktop |
| --- | --- |
| Move | Left / Right (A / D aliases) |
| Rotate clockwise | Up (W alias) |
| Rotate counterclockwise | Z |
| Soft drop | Down (S alias) |
| Hard drop | Space |
| Hold | C or Shift |
| Chat | Tab |
| Ready in room | R |
| Start round as host | GUI start control |
| Pause | F5 |

GUI key bindings can be changed in its keymap settings. During play, number keys target player slots for specials; use the displayed inventory and target indicators. Use `/help` in supported chat/console contexts for command help.

If a connection drops during a round, the server reserves the player's slot for `reconnect_window` seconds (default 15). Desktop clients retry automatically, reauthenticate, and request the same round/slot while preserving their local active piece. A countdown is shown; Esc cancels recovery in the desktop client. Gameplay is frozen during recovery. Kicks and authentication failures stop retries. Reconnecting after expiry or after a server restart does not restore the active round. Saved room preferences and account statistics survive restarts.

A silent connection is treated as lost after five seconds without a reply. The grace countdown starts when the client detects the loss; the server reservation starts when it observes the old connection close. Expired slots, a changed round, account disappearance or server restart require a manual choice rather than silently starting a new game. See [trust review](TRUST-REVIEW.md) and [platform verification](PLATFORM-VERIFICATION.md).

The GUI music player includes the title theme and an 18-track in-game library, including all ten cyberpunk-pack pieces. Open **Music Player** beside Hold/Next during an active round. The main menu plays only the title theme; leaving a round closes the player. It has track selection, seek, previous/next, pause, shuffle, volume and attribution links. About lists all music credits across pages. See [soundtrack details](../music/README.md) and [credits](../CREDITS.md).

See [discovery, ranked history, health reports, replays and music shortcuts](ENHANCEMENTS.md) for the latest controls, deployment changes and verification limits.
