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

CREATE TABLE IF NOT EXISTS directory_registration_limits (
 source_hash CHAR(64) CHARACTER SET ascii PRIMARY KEY,
 window_start BIGINT NOT NULL, requests INT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS accounts (
 id CHAR(32) CHARACTER SET ascii PRIMARY KEY,
 handle VARCHAR(14) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL UNIQUE,
 -- recovery_hash is retained for schema compatibility; codes are not issued or accepted.
 password_hash VARCHAR(255) NOT NULL, recovery_hash CHAR(64) CHARACTER SET ascii NOT NULL,
 active TINYINT NOT NULL DEFAULT 1, created_at BIGINT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS account_devices (
 request_hash CHAR(64) CHARACTER SET ascii PRIMARY KEY,
 code CHAR(8) CHARACTER SET ascii NOT NULL UNIQUE,
 server_id VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 account_id CHAR(32) CHARACTER SET ascii NULL,
 expires BIGINT NOT NULL,state VARCHAR(16) NOT NULL,
 FOREIGN KEY(server_id) REFERENCES directory_servers(id),
 FOREIGN KEY(account_id) REFERENCES accounts(id), INDEX expiry(expires)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

ALTER TABLE accounts ADD COLUMN IF NOT EXISTS auth_version INT NOT NULL DEFAULT 1;

-- Native-client sessions contain only hashes of high-entropy bearer tokens.
CREATE TABLE IF NOT EXISTS account_sessions (
 token_hash CHAR(64) CHARACTER SET ascii PRIMARY KEY,
 account_id CHAR(32) CHARACTER SET ascii NOT NULL,
 auth_version INT NOT NULL, expires BIGINT NOT NULL, revoked TINYINT NOT NULL DEFAULT 0,
 FOREIGN KEY(account_id) REFERENCES accounts(id), INDEX expiry(expires)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Only hashes of single-use reset tokens are stored; one outstanding link per account.
CREATE TABLE IF NOT EXISTS account_password_resets (
 account_id CHAR(32) CHARACTER SET ascii PRIMARY KEY,
 token_hash CHAR(64) CHARACTER SET ascii NOT NULL UNIQUE,
 auth_version INT NOT NULL, expires BIGINT NOT NULL,
 FOREIGN KEY(account_id) REFERENCES accounts(id), INDEX expiry(expires)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Browser PIN login for a native client, separate from server connection approvals.
CREATE TABLE IF NOT EXISTS account_client_pins (
 device_hash CHAR(64) CHARACTER SET ascii PRIMARY KEY,
 pin CHAR(8) CHARACTER SET ascii NOT NULL UNIQUE,
 client VARCHAR(16) CHARACTER SET ascii NOT NULL,
 account_id CHAR(32) CHARACTER SET ascii NULL,
 auth_version INT NULL, expires BIGINT NOT NULL,
 last_poll BIGINT NOT NULL DEFAULT 0, state VARCHAR(16) NOT NULL DEFAULT 'pending',
 FOREIGN KEY(account_id) REFERENCES accounts(id), INDEX expiry(expires)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Existing accounts keep username sign-in until an email is added.
ALTER TABLE accounts ADD COLUMN IF NOT EXISTS email VARCHAR(254) CHARACTER SET ascii COLLATE ascii_general_ci NULL;
ALTER TABLE accounts ADD UNIQUE INDEX IF NOT EXISTS accounts_email(email);


-- Existing accounts retain access; only opted-in new registrations await activation.
ALTER TABLE accounts ADD COLUMN IF NOT EXISTS email_pending TINYINT NOT NULL DEFAULT 0;
CREATE TABLE IF NOT EXISTS account_email_verifications (
 account_id CHAR(32) CHARACTER SET ascii PRIMARY KEY,
 token_hash CHAR(64) CHARACTER SET ascii NOT NULL UNIQUE,
 email VARCHAR(254) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,
 expires BIGINT NOT NULL,
 FOREIGN KEY(account_id) REFERENCES accounts(id), INDEX expiry(expires)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
