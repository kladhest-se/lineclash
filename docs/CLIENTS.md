# Run and configure the clients

## Desktop client

Download a package for your platform from **Get the game** on the home page and extract the complete archive. Keep the executable with its bundled libraries and music directory. From an extracted native package, run `bin/lineclash` on macOS or Linux, or `bin/lineclash.exe` on Windows. From a source build, run `./clients/gui/lineclash` (`clients/gui/lineclash.exe` on Windows). Build instructions are on [Source](DEVELOPMENT.md).

In the main menu, **Public servers** loads the official directory from `https://lineclash.kladhest.se/api/`. **Local network** searches nearby servers directly over UDP and uses their LAN address. **Saved servers** contains your locally stored endpoints and offers Add, Edit and Remove controls. Use Saved servers for private hosts and ports. You can save or edit an endpoint while it is offline; account credentials are optional until connecting. Public entries cannot be renamed or removed from the directory by a client. Favourite a public endpoint to keep a local copy. The default game port is **31457**. For a server on this computer, use `localhost`. Enter your nickname and password, then connect. New names register when the server allows registration; existing names require their original password. Accounts belong to each server.

Select a room, choose a team if wanted, and ready up. The room host controls rules, bots and starting the round. Leave the room to return to the lobby; disconnect before choosing another server. Use **Quit** in the main menu or close the window to stop the desktop client.

## Desktop preferences and saved servers

Use **Settings → Controls** to change gameplay bindings and display preferences. Music and SFX controls set their respective volume and enabled state. The menu plays the title theme; during a round, **Music Player** provides track selection, pause, seek and shuffle. Track attribution is available in the player, About and the website Credits page.

Preferences and saved server entries are stored locally:

| Platform | Preferences | Saved servers |
| --- | --- | --- |
| macOS / Linux | `~/.lineclash_settings` | `~/.lineclash_servers` |
| Windows | `%APPDATA%\LineClash\settings` | `%APPDATA%\LineClash\servers` |

Close the client before backing up or changing these files. Saved authentication hashes can grant account access: keep them private. To reset preferences, close the game and rename the settings file; the next launch uses defaults. Keep the original file if you want to restore your bindings later.

## Connection problems

Check the server host and game port first; the HTTP status port is not a game endpoint. **Unreachable** usually requires checking that the server is running, its address is correct and the game port is reachable through the firewall. **Wrong credentials** requires the correct password for that server. **Server full** requires a free slot, while **Protocol mismatch** requires a compatible client/server version. Access-denied or banned accounts require assistance from that server's administrator.

During an active-game connection loss, automatic reconnect freezes local play while the server reserves your slot. Escape cancels recovery in the desktop client. An expired reservation, completed round or server restart requires reconnecting normally. See [game basics](GAME-BASICS.md).

## Central LineClash accounts

Public-server joins use central browser approval. The client displays an eight-character code and opens `https://lineclash.kladhest.se/account/`. Create an account or sign in, verify the server name/address and approve only the code currently shown in your game. You can use a browser on another device. A browser session lets you approve later connections without entering the password again.

The website stores a password hash and provides a recovery code once when registering. Keep that code private. Recovery changes the password, rotates the recovery code and invalidates earlier browser sessions. The game server never receives the central password or a reusable central login token.

Central handles appear as `@handle` and have an immutable account ID. They are mapped separately on each server; an existing local account with a matching name is never claimed automatically. Each server keeps control of its own permissions, bans and local game records. No existing local statistics are automatically transferred to a central profile.

The Account button opens your website profile. Private/LAN play still supports local accounts and does not require the central website. Older clients can keep using local accounts on updated servers. Central approval requires a verified, live public directory identity for the server. During a central-service outage, existing games continue and local-account play remains available; new central sign-ins are unavailable.

The game TCP stream remains unencrypted. Browser authentication and server verification use HTTPS; chat and gameplay can be observed or altered on an untrusted network. A reconnect token is restricted to a reserved slot on one server and expires with that short reconnect window; it is not a central account credential.

## Native AmigaOS 3.x preview

The experimental native frontend targets a 68020 without an FPU, AmigaOS 3.x and a running bsdsocket.library TCP/IP stack. It opens a 640×256, sixteen-colour Intuition screen. Use arrows, Space, Z, C, Tab, number keys and the mouse. Central approval displays the same website URL/code and can be completed on another device. Public discovery uses AmiSSL certificate and hostname verification in a separate process. The initial native frontend has no music/SFX playback or modern gamepad integration. See the repository's Amiga guide for build and emulator checks.
