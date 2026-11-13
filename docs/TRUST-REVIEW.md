# Client trust review

## Boundaries tightened

- Room identity and privilege lookup are bound to the account authenticated on that socket. The join nickname cannot impersonate another account.
- Field updates require the sender's slot, exactly 264 cells, and supported cell values.
- Gameplay handlers check active round, living player, spectator status and pause state before applying any updates. Paused players are exempt from field-heartbeat kicks.
- Special requests require a valid living target and the exact next server-issued inventory item. Empty inventory cannot synthesize a self-special, substitutions do not consume inventory, and the sender slot is taken from the socket rather than the packet.
- Client level claims do not raise server-owned levels.
- Resume requests identify a server-issued round ID and an authenticated, unexpired reserved slot. Resuming does not increment the held player count. Paused state, roster, field snapshot and special inventory are resynchronized.

Regression tests exercise these boundaries with adversarial raw TCP clients. This is protocol hardening, not a claim that gameplay is fully authoritative.

## Remaining architectural trust

The existing protocol still accepts a client's board contents and `lineclear 1..4` reports. A modified client can fabricate plausible boards/clears and thereby earn garbage attacks, specials and progression. The server's pre-generated piece queue is not yet a complete validated input simulation. Hold/reset messages and client-side effects also rely on honest gameplay. Plain TCP does not provide transport confidentiality, and account password hashes are authentication credentials.

For cheat-resistant ranked matches, introduce a versioned input protocol: clients send sequenced movement/rotation/drop/hold commands, the server simulates piece motion, locks, clears, hold state and garbage, and authoritative snapshots correct client prediction. Derive awards/statistics solely from that simulation and record command sequences for replay. Keep the legacy board-reporting protocol explicitly unranked until that migration is complete. This requires a coordinated gameplay protocol change rather than speculative validation of legacy board snapshots.
