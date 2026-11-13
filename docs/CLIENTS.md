# Run and configure the clients

## Desktop client

Download a package for your platform from **Get the game** on the home page and extract the complete archive. Keep the executable with its bundled libraries and music directory. From an extracted native package, run `bin/lineclash` on macOS or Linux, or `bin/lineclash.exe` on Windows. From a source build, run `./clients/gui/lineclash` (`clients/gui/lineclash.exe` on Windows). Build instructions are on [Source](DEVELOPMENT.md).

In the main menu, select **Find servers**, choose a saved server, or add a host and port manually. The default game port is **31457**. For a server on this computer, use `localhost`. Enter your nickname and password, then connect. New names register when the server allows registration; existing names require their original password. Accounts belong to each server.

Select a room, choose a team if wanted, and ready up. The room host controls rules, bots and starting the round. Leave the room to return to the lobby; disconnect before choosing another server. Use **Quit** in the main menu or close the window to stop the desktop client.

## Desktop preferences and saved servers

Use **Keymap** to change gameplay bindings and display preferences. Music and SFX controls set their respective volume and enabled state. The menu plays the title theme; during a round, **Music Player** provides track selection, pause, seek and shuffle. Track attribution is available in the player, About and the website Credits page.

Preferences and saved server entries are stored locally:

| Platform | Preferences | Saved servers |
| --- | --- | --- |
| macOS / Linux | `~/.lineclash_settings` | `~/.lineclash_servers` |
| Windows | `%APPDATA%\LineClash\settings` | `%APPDATA%\LineClash\servers` |

Close the client before backing up or changing these files. Saved authentication hashes can grant account access: keep them private. To reset preferences, close the game and rename the settings file; the next launch uses defaults. Keep the original file if you want to restore your bindings later.

## Terminal client — macOS and Linux

The terminal client is available from a source build on macOS and Linux. It is not part of the native Windows client target. Open a terminal and run:

```sh
./clients/cli/lineclash-cli localhost 31457
```

For saved connections, see the options supported by your installed version:

```sh
./clients/cli/lineclash-cli --help
./clients/cli/lineclash-cli --list
./clients/cli/lineclash-cli --connect YOUR_SAVED_ALIAS
```

`--cfg PATH` selects a separate credentials file. The help output shows its default location. Adding profiles with `--password` or supplying a password as a positional argument can expose it through shell history and process listings. Protect saved credentials as carefully as passwords.

Follow the contextual footer for each screen. In a room, **R** readies, **S** starts as host, **T** edits the team, **E/M/H** adds an easy/medium/hard bot, **X** removes a bot and **Q** leaves. During play, arrows move, Up/Z rotate, Down soft-drops, Space hard-drops, C holds and Tab opens chat. Number keys target specials. Use the contextual quit/leave command to exit.

The terminal client uses the legacy board-reporting protocol; rounds involving it are casual. The desktop client supports server-processed inputs for eligible ranked rounds.

## Connection problems

Check the server host and game port first; the HTTP status port is not a game endpoint. **Unreachable** usually requires checking that the server is running, its address is correct and the game port is reachable through the firewall. **Wrong credentials** requires the correct password for that server. **Server full** requires a free slot, while **Protocol mismatch** requires a compatible client/server version. Access-denied or banned accounts require assistance from that server's administrator.

During an active-game connection loss, automatic reconnect freezes local play while the server reserves your slot. Escape cancels recovery in the GUI; Q/Escape cancels in the terminal. An expired reservation, completed round or server restart requires reconnecting normally. See [game basics](GAME-BASICS.md).
