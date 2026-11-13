# Playing LineClash

Start the server, then open `./clients/gui/lineclash` or run `./clients/cli/lineclash-cli localhost`. In the GUI, add/select the host and port, enter a nickname and password, and connect. A new nickname creates an account unless registration is disabled. Reuse the same credentials to log in later. Choose a room from the lobby list.

The first player in a room becomes host. Ready players can start a round using the room's start control; the host can manage bots and room rules. Players on the same named team cooperate. Set or clear the team in the lobby. Room and server chat are separate; the GUI exposes both and room settings through its controls. Spectators watch without playing; use the GUI room controls to spectate and arrows to change the watched player. An eliminated player can also watch opponents.

## Default desktop controls

| Action | GUI | Terminal |
| --- | --- | --- |
| Move | Left / Right (A / D aliases) | Left / Right |
| Rotate clockwise | Up (W alias) | Up |
| Rotate counterclockwise | Z | Z |
| Soft drop | Down (S alias) | Down |
| Hard drop | Space | Space |
| Hold | C or Shift | C |
| Chat | Tab | Tab |
| Ready in room | R | R |
| Start round as host | GUI start control | S in room |
| Pause | F5 | See in-game footer / room permissions |

GUI key bindings can be changed in its keymap settings. The terminal displays contextual shortcuts; in its room lobby, T edits the team, E/M/H adds an easy/medium/hard bot, X removes a bot, and Q leaves the room. Room hosts/managers can use 1–6 to change the listed room rules. During play, number keys target player slots for specials; use the displayed inventory and target indicators. Use `/help` in supported chat/console contexts for command help.

If a connection drops during a round, the server reserves the player's slot for `reconnect_window` seconds (default 15). GUI and terminal clients now retry automatically, reauthenticate, and request the same round/slot while preserving their local active piece. A countdown is shown; Esc cancels in the GUI and Q/Esc cancels in the terminal. Gameplay is frozen during recovery. Kicks and authentication failures stop retries. Reconnecting after expiry or after a server restart does not restore the active round. Saved room preferences and account statistics survive restarts.

A silent connection is treated as lost after five seconds without a reply. The grace countdown starts when the client detects the loss; the server reservation starts when it observes the old connection close. Expired slots, a changed round, account disappearance or server restart require a manual choice rather than silently starting a new game. See [trust review](TRUST-REVIEW.md) and [platform verification](PLATFORM-VERIFICATION.md).

The GUI music player includes the title theme and an 18-track in-game library, including all ten cyberpunk-pack pieces. Open **Music Player** beside Hold/Next during an active round. The main menu plays only the title theme; leaving a round closes the player. It has track selection, seek, previous/next, pause, shuffle, volume and attribution links. About lists all music credits across pages. See [soundtrack details](../music/README.md) and [credits](../CREDITS.md).

See [discovery, ranked history, health reports, replays and music shortcuts](ENHANCEMENTS.md) for the latest controls, deployment changes and verification limits.
