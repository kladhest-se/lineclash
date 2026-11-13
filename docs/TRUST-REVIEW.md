# Client trust review

## Boundaries tightened

- Room identity and privilege lookup are bound to the account authenticated on that socket. The join nickname cannot impersonate another account.
- Field updates require the sender's slot, exactly 264 cells, and supported cell values.
- Gameplay handlers check active round, living player, spectator status and pause state before applying any updates. Paused players are exempt from field-heartbeat kicks.
- Special requests require a valid living target and the exact next server-issued inventory item. Empty inventory cannot synthesize a self-special, substitutions do not consume inventory, and the sender slot is taken from the socket rather than the packet.
- Client level claims do not raise server-owned levels.
- Resume requests identify a server-issued round ID and an authenticated, unexpired reserved slot. Resuming does not increment the held player count. Paused state, roster, field snapshot and special inventory are resynchronized.

Regression tests exercise these boundaries with adversarial raw TCP clients. This is protocol hardening, not a claim that gameplay is fully authoritative.

## Authoritative input protocol

Updated GUI clients negotiate `input-v1`. The server simulates gravity, moves, rotation, holds, locks, clears and inventory. Client board/line/level/loss claims are ignored in that mode. Inputs require strictly increasing sequence numbers, bounded processing rates, server-timed drops and bounded lock resets. Reconnect snapshots restore the server state. Legacy clients remain playable but cannot create ranked rounds. Older ranked history without `input-v1` provenance is demoted to casual during local and website migrations.

## Remaining architectural trust

Operators can fabricate their own reports, and automation, collusion and account sharing are not prevented by input simulation. Plain TCP still lacks transport confidentiality; that work was not selected in this change. Snapshot replays contain server-derived boards, not a deterministic command-resimulation proof. Native Windows/Linux execution still requires their CI hosts.
