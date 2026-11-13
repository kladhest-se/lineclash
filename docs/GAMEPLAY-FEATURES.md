# Servers, match history and replays

## Finding a server

Select **Find servers** in the native server browser, or copy an address from the website’s Servers page. Add the host and port, enter your nickname and password, and connect. Credentials are local to each server; do not assume the same nickname is the same person on different servers.

Use All, Favourites, Recent, Open slots or Ping ≤100 ms to narrow the native list. You can sort by latency or recent play. Unknown latency sorts last; an unknown capacity is not a promise of free slots. Favourite servers are saved locally, and recent entries are recorded after a successful connection.

Discovery does not send your account credentials to the directory. Selecting a new endpoint opens the connection form; saved credentials are reused only for the matching server and nickname.

## Ranked and casual games

A round can award ranked wins when it starts with at least two human players and no active bots. Manually stopped rounds, solo games and bot games are casual. Natural draws can count as ranked matches without awarding a winner. Ranked results are reported by the server; they are not a guarantee of tournament rules or cheat-free play.

Statistics includes match history, player records and ranked wins. Player records are scoped to the server and nickname. A placement of zero means unknown, such as after a disconnect. New timestamps use UTC; old history is labelled legacy rather than assigned a guessed timezone.

## Watching a replay

Open a match from Statistics and choose its details or replay. Available replays support play/pause, seeking, playback speed and board snapshots. You can download the recorded events as JSON. Names link to the player’s history on that server.

A replay shows recorded gameplay snapshots rather than a frame-perfect video. Partial traces are labelled, and older matches may have no recording. Authentication and chat are excluded from recorded gameplay. A checksum detects damaged or changed data; it does not prove that a server operator reported an authentic game.
