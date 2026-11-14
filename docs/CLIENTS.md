# Run and configure the clients

## Desktop client

Download a package for your platform from **Get the game** on the home page and extract the complete archive. Keep the executable with its bundled libraries and music directory. From an extracted native package, run `bin/lineclash` on macOS or Linux, or `bin/lineclash.exe` on Windows. From a source build, run `./clients/gui/lineclash` (`clients/gui/lineclash.exe` on Windows). Build instructions are on [Source](DEVELOPMENT.md).

In the main menu, **Public servers** loads the official directory from `https://lineclash.kladhest.se/api/`. **Local network** searches nearby servers directly over UDP and uses their LAN address. **Saved servers** contains your locally stored endpoints and offers Add, Edit and Remove controls. Use Saved servers for private hosts and ports. You can save or edit an endpoint while it is offline; account credentials are optional until connecting. Public entries cannot be renamed or removed from the directory by a client. Favourite a public endpoint to keep a local copy. The default game port is **31457**. For a server on this computer, use `localhost`. Public-server joins use the selected central account. Private/offline connections can use local nicknames and passwords; those accounts belong to that server and are created when it allows registration.

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

Open **Account** in the main menu and sign in with your central email and password. The client immediately uses that identity for public servers; no profile selection or extra confirmation is needed. If you join a public server while signed out, sign-in continues directly into the selected server. Open Account again to see your username and email or sign out. Sign out before entering another account. The client remembers only the last successful identity.

Alternatively, choose **Sign in with a PIN**. The client displays an eight-digit PIN and the unlinked page `https://lineclash.kladhest.se/pin/`. Open that page on any computer or phone, enter the PIN first, then enter your username or email and password and choose **Sign in and authenticate client**. Confirm that the PIN matches your own game window before approving. The game checks for approval automatically and continues using the same central account session as password login. The PIN expires after five minutes and can be redeemed once. **Use username / password instead** or Escape cancels the client’s wait; a late approval cannot sign it in. Use **Get a new PIN** after expiry or a failed request. PINs are temporary client approvals, not passwords or recovery codes, and are never sent to game servers.

Choose **Create a new account** in the client, enter a public username, private email, password and matching confirmation, then choose **Create account**. If the website requires email activation, the client asks you to check your inbox instead of signing in. Open the activation link to verify your email and sign in to the website automatically, then sign in normally in the client. Use **Resend activation** or `https://lineclash.kladhest.se/account/verify/` if the email did not arrive. Links expire after 24 hours and can be used once. When activation is disabled, registration signs you in immediately and returns to the server browser or continues the pending server connection. Recovery codes are no longer issued or accepted. Registration starts inside the client; optional email confirmation uses the website. Username and email must each be unique; the website’s configured password minimum applies. Email is used for sign-in; older accounts without email can still use their username. **Forgot password?** opens `https://lineclash.kladhest.se/account/recover/`. When the operator has configured SMTP, enter your registered email to receive a single-use reset link that expires after 30 minutes. Choose and confirm a new password on the website, then sign in again in the client. If SMTP is not configured, the recovery page explains that recovery is disabled. Accounts without an email cannot use email recovery. New clients do not require browser approval codes. The desktop and experimental Amiga client send login credentials only to the official HTTPS API with certificate and hostname verification.

The client remembers one username and private email address: `~/.lineclash_accounts` on macOS/Linux, `%APPDATA%\LineClash\accounts` on Windows and `PROGDIR:LineClash.accounts` on AmigaOS. Older saved-profile files are read for their first valid entry; the next successful sign-in replaces the file with one identity. Passwords are cleared after each request. Login sessions stay in memory, last up to twelve hours and can be reused across verified public servers; sign in again after restarting the client. Sign-out revokes that client session and does not interrupt games already joined.

Each game connection receives a fresh challenge. The client approves it over HTTPS using the signed-in account session, bound to the selected public server address and port. The game server polls its own request to obtain the verified identity. Neither a central password nor the reusable central session token travels over the unencrypted gameplay connection. Older clients can continue using browser-code approval on updated servers.
The website stores a password hash. An email password reset invalidates earlier browser sessions, native sessions and pending approved connections; it does not disconnect games already joined. Requesting a reset alone does not change the password. A newer reset email replaces the previous link. The game server never receives the central password or a reusable central login token.

Central handles appear as `@handle` and have an immutable account ID. They are mapped separately on each server; an existing local account with a matching name is never claimed automatically. Each server keeps control of its own permissions, bans and local game records. No existing local statistics are automatically transferred to a central profile.

The Account button opens the sign-in form or your signed-in account. Private/LAN play still supports local accounts and does not require the central website. Older clients can keep using local accounts on updated servers. Central approval requires a verified, live public directory identity for the server. During a central-service outage, existing games continue and local-account play remains available; new central sign-ins are unavailable.

The game TCP stream remains unencrypted. Browser authentication and server verification use HTTPS; chat and gameplay can be observed or altered on an untrusted network. A reconnect token is restricted to a reserved slot on one server and expires with that short reconnect window; it is not a central account credential.

## Website profile settings and deletion

Sign in to the website's **Account** page to choose a saved block avatar, view your reported public match totals, or change your email or password. The avatar and username in the top menu open your profile; **Sign out** in that menu ends the website session. Sign-up and recovery links are hidden while signed in. Avatars identify your website profile; they do not change the game client's appearance. Each change requires your current password. The username remains the stable game identity. If the operator requires email verification, an email change sends a single-use, 24-hour confirmation link to the new address; your current email stays active until you confirm. Other sign-ins are revoked after a password change or completed email change. The current browser stays signed in after a direct settings change; emailed confirmation requires signing in again. Mail notifications are sent to the old address when configured. Sign in again in the game with the new details or your username.

To remove your central account, open **Delete my account permanently**, enter your current password and type `DELETE`. This permanently erases the live website's account, email, password hash, sign-in sessions and pending account tokens. Your identity is removed from website player leaderboards, statistics and replays; anonymous results display as **DELETED PLAYER**. Match scores, placements, line/special/block counts, wins and team outcomes remain. Multiple deleted players in the same match keep separate rows. Replays retain board data and timing, with refreshed integrity checksums after name removal. A successful deletion signs you out and cannot be undone.

This covers the central website. Independently operated game servers retain their own local accounts, records and logs; already downloaded replays and operator backups have separate retention. Existing game connections may continue until they end, but new central sign-ins stop working. The client may still remember a username/email locally; those files are under your control. Contact the relevant operator for data held outside the central website.

## Native AmigaOS 3.x preview

The experimental native frontend targets a 68020 without an FPU, AmigaOS 3.x and a running bsdsocket.library TCP/IP stack. It opens a 640×256, sixteen-colour Intuition screen. Use arrows, Space, Z, C, Tab, number keys and the mouse. The Account screen uses the same single identity and direct HTTPS sign-in through AmiSSL. Account creation is available directly in the client; email password resets use the website when SMTP is configured. Public discovery uses AmiSSL certificate and hostname verification in a separate process. The initial native frontend has no music/SFX playback or modern gamepad integration. See the repository's Amiga guide for build and emulator checks.
