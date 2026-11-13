CREATE TABLE IF NOT EXISTS directory_servers (
 id VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin PRIMARY KEY,
 name VARCHAR(128) NOT NULL, host VARCHAR(253) NOT NULL, port INT UNSIGNED NOT NULL,
 token_hash CHAR(64) CHARACTER SET ascii NOT NULL, last_seen BIGINT NOT NULL DEFAULT 0,
 status_json MEDIUMTEXT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS directory_games (
 server_id VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 database_id CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 game_id BIGINT UNSIGNED NOT NULL, room_id INT NOT NULL, started_at VARCHAR(32) NOT NULL,
 ended_at VARCHAR(32) NOT NULL, winner_nick VARCHAR(64) NOT NULL,
 winner_team VARCHAR(64) NOT NULL, player_count INT NOT NULL,
 PRIMARY KEY(server_id,database_id,game_id), INDEX recent(ended_at),
 FOREIGN KEY(server_id) REFERENCES directory_servers(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Additive and repeatable upgrades. Legacy games remain casual with unknown epoch.
ALTER TABLE directory_games ADD COLUMN IF NOT EXISTS started_epoch BIGINT NOT NULL DEFAULT 0;
ALTER TABLE directory_games ADD COLUMN IF NOT EXISTS ended_epoch BIGINT NOT NULL DEFAULT 0;
ALTER TABLE directory_games ADD COLUMN IF NOT EXISTS ranked TINYINT NOT NULL DEFAULT 0;
ALTER TABLE directory_games ADD COLUMN IF NOT EXISTS end_reason VARCHAR(32) NOT NULL DEFAULT 'legacy';
ALTER TABLE directory_games ADD COLUMN IF NOT EXISTS replay_json MEDIUMTEXT NULL;
ALTER TABLE directory_games ADD COLUMN IF NOT EXISTS replay_sha256 CHAR(64) NOT NULL DEFAULT '';
ALTER TABLE directory_games ADD COLUMN IF NOT EXISTS replay_truncated TINYINT NOT NULL DEFAULT 0;
CREATE TABLE IF NOT EXISTS directory_players (
 server_id VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 database_id CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 game_id BIGINT UNSIGNED NOT NULL, nick VARCHAR(64) NOT NULL,
 lines_cleared INT UNSIGNED NOT NULL, specials_sent INT UNSIGNED NOT NULL,
 blocks_placed INT UNSIGNED NOT NULL, placement INT UNSIGNED NOT NULL, won TINYINT NOT NULL,
 PRIMARY KEY(server_id,database_id,game_id,nick), INDEX player_history(server_id,nick),
 FOREIGN KEY(server_id,database_id,game_id) REFERENCES directory_games(server_id,database_id,game_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Legacy board reports cannot establish authoritative ranked results.
ALTER TABLE directory_games ADD COLUMN IF NOT EXISTS input_protocol VARCHAR(16) NOT NULL DEFAULT 'legacy';
UPDATE directory_games SET ranked=0 WHERE input_protocol<>'input-v1';
