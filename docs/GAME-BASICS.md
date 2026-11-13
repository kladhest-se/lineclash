# Game basics

LineClash is a multiplayer falling-block game. Move and rotate each falling piece, build complete horizontal lines, and keep your stack below the top of the field. Cleared lines make room for more pieces and can earn attacks or specials, depending on the room rules.

## Join your first game

Get a client for your platform, open the server browser, and select a server or add its host and port. Enter a nickname and password. When registration is enabled, a new nickname creates an account on that server. Reuse those credentials when returning to that server.

Choose a room and ready up. The room host starts the round and controls its rules and bots. Your nickname and statistics belong to that server; the same name elsewhere does not establish the same identity.

## Read the playing field

Your field shows the active piece and its landing preview. Next shows the upcoming piece; Hold stores a piece for later when enabled. Hold is usable once per piece, then resets after a lock. Opponent boards show how the rest of the room is doing.

The specials inventory shows available items. Select an opponent or your own slot to use the next special when attacks are enabled. A topped-out player is eliminated and can watch the remaining players.

## Reconnecting

During a connection loss, the game freezes locally and retries automatically while the server reserves your slot, normally for 15 seconds. Escape cancels recovery. A completed round, expired reservation, failed login or restarted server cannot restore the old active game. Updated clients restore the server’s authoritative board and active piece after reconnecting.

See [controls](CONTROLS.md), [game modes](GAME-MODES.md) and [specials](SPECIALS.md) for the next steps.

## How pieces, clears and levels work

Each piece is made of four blocks. Move it sideways and rotate it while it falls. Soft drop brings it down faster; hard drop places it immediately at the landing position. Once a piece locks, it becomes part of the stack and the next piece enters the field. A complete horizontal row clears, and the blocks above it move down. Leaving holes makes later clears harder; reaching the top can eliminate you.

The ghost outline previews a landing without placing the piece. Hold exchanges the active piece with the stored piece, or stores it and draws the next piece when Hold is empty. It is available only when the room enables it, and normally once between locks. Rotation can move a piece slightly around an obstruction when a supported wall kick fits; a blocked rotation simply fails.

Cleared lines contribute to your line count and progression. The starting level, lines required per increase and increase size are room settings. Faster levels reduce the time available to place pieces. Score, lines and survival describe different aspects of a round; the room's win condition determines its result.

## Attacks and room rules

Rooms can enable specials, classic or guideline garbage, and different targeting rules. Garbage adds occupied rows and raises the stack. Guideline attack rules can reward combinations such as consecutive clears, back-to-back difficult clears and a perfect clear; configured bonuses vary by server. Learn the room's rules before comparing results across servers.

Specials are separate inventory items earned according to the room rules. Their effects can help you or disrupt opponents; choose the displayed target carefully. See [specials](SPECIALS.md) for every effect. Long-running rounds can gain speed-up garbage when enabled, so a safe stack can become dangerous even without a new opponent attack.

Before starting, check the room settings, team name, player readiness and any bots. A paused game waits for play to resume; pause permissions depend on the room. A disconnected player has only the configured reconnect window to return. Elimination, voluntarily leaving and stopping the server are not ways to save a round for later.
