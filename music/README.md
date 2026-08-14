# LineClash — Music Drop Folder

Place your MOD music files here:

| File | When it plays |
|------|--------------|
| `lineclash_menu.mod` | Server list, lobby, and all menus |
| `lineclash_game.mod` | During a game round |

All three packaging scripts (`package-macos.sh`, `package-windows.sh`,
`package-amiga.sh`) look here first and automatically bundle whatever
files they find. Files are silently omitted if missing — the client
runs fine without music.

Both files must be standard 4-channel ProTracker MOD format (M.K. tag).
A good source for public-domain Amiga MODs is https://modarchive.org
