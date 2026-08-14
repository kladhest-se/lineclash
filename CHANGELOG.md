# Changelog

## v0.9 (current)

### Gameplay
- **Guideline garbage system** — Back-to-back bonus, combos, perfect clear (10 lines), configurable via `server.cfg`
- **Pending garbage queue** — In guideline mode, incoming garbage is queued and flushed on lock; cleared lines cancel incoming (counter-attack)
- **Garbage targeting** — All / Random / Focused modes, per-room settable
- **Hold piece** — C or Shift; resets each new piece; shown in sidebar with key hint
- **SRS wall kicks** — Full Super Rotation System for all pieces
- **Lock delay** — Configurable 0–500ms; lateral and rotation moves reset the timer
- **Win streaks** — Fire indicator when a player wins consecutive rounds
- **Reconnect window** — Disconnected players' slots held for configurable seconds

### Bots
- Bots now use all game rules: guideline garbage, `lines_per_special`, B2B/combos, garbage targeting
- Full special switch — all 10 special types including switch field, block bomb, clear column
- Self-beneficial specials target self; offensive specials use room targeting mode
- Difficulty scales speed, mistake rate, and special-fire frequency

### Server
- **Persistent room settings** — All 6 room options saved to DB and restored on restart
- **Fine-grained config** — `btb_bonus`, `combo_cap`, `perfect_clear_bonus`, `garbage_hole_style`
- **Stats API** — Room settings, garbage config, and lock delay now included in `/api/status`

### GUI Client
- HiDPI / Retina fix — `SDL_RenderSetLogicalSize` ensures full-window rendering on Mac
- Lobby list — Two-line room rows with badges for all 6 game options including garbage mode and targeting
- Game options popup — Clickable during `CS_GAMEOVER` state; `Label [ON/OFF]` format; wider popup; full-width Close button
- Popup blocking — Hover and click suppressed on background while any popup is open
- Hold box shows `HOLD [C]` key hint

### CLI Client
- Lobby list shows CLASSIC/GUIDELINE and TARGET mode columns
- Keys `[5]` / `[6]` toggle garbage system and targeting in lobby

---

## v0.8

- Initial developer release with auth, rooms, specials, bots, leaderboard, spectator mode, sound, team play
