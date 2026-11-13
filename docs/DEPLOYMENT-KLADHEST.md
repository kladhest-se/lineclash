# Deploy lineclash.kladhest.se — Leap 16 / Apache / remote MariaDB

This guide is tailored to your topology. Commands are instructions to run on the named hosts; no remote systems were modified while preparing it.

| Role | Host | Private address |
|---|---|---|
| Apache/PHP website | sth-ts-web002.kladhest.se | 172.16.1.132 |
| MariaDB | sth-ts-dbs001.kladhest.se | 172.16.1.121 |
| Dedicated game server | sth-ts-mgt001.kladhest.se | 172.16.1.100 |

The website is `/srv/www/vhosts/lineclash.kladhest.se`. Website configuration is private at `/srv/www/vhosts/lineclash.kladhest.se/config.inc.php`. The game server uses local SQLite and sends results to `https://lineclash.kladhest.se/api/`; it never needs a MariaDB login.

## 1. On dbs001: create the database and account

Use an administrative MariaDB session (`mariadb -u root -p`, or your existing socket-authenticated root command). Choose a new long password; the placeholder below is not a usable password:

```sql
CREATE DATABASE lineclash CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'lineclash_web'@'172.16.1.132'
  IDENTIFIED BY 'REPLACE_WITH_A_NEW_LONG_RANDOM_PASSWORD' REQUIRE SSL;
GRANT SELECT, INSERT, UPDATE, CREATE, ALTER, INDEX, REFERENCES
  ON lineclash.* TO 'lineclash_web'@'172.16.1.132';
SHOW GRANTS FOR 'lineclash_web'@'172.16.1.132';
```

`CREATE/ALTER/INDEX/REFERENCES` are needed for schema installation/upgrades. No global privileges or remote root account are needed. Do not use `'%'` as the user host. `FLUSH PRIVILEGES` is unnecessary after these account statements.

Check the existing listener/TLS setup before changing a shared database service:

```sql
SHOW VARIABLES LIKE 'bind_address';
SHOW VARIABLES LIKE 'port';
SHOW VARIABLES LIKE 'have_ssl';
SHOW VARIABLES LIKE 'ssl_cert';
```

The service must listen on 172.16.1.121:3306 and support TLS. Keep an already working listener/certificate setup. If it currently listens only on loopback, adjust its configuration for your private interface. Example `/etc/my.cnf.d/lineclash-network.cnf` settings (use the actual paths to certificates issued by your trusted CA):

```ini
[mariadb]
bind-address=172.16.1.121
ssl-ca=/etc/mysql/tls/ca.pem
ssl-cert=/etc/mysql/tls/sth-ts-dbs001-fullchain.pem
ssl-key=/etc/mysql/tls/sth-ts-dbs001-key.pem
```

The server certificate must cover **sth-ts-dbs001.kladhest.se** in its SAN. Make the private key readable by MariaDB's service user and keep it on dbs001. Only copy the CA certificate to web002. If adding this file, check for conflicting existing settings and schedule `systemctl restart mariadb` appropriately for other hosted databases. `SHOW VARIABLES LIKE 'have_ssl'` should report `YES` afterward.

If firewalld is used, identify the actual interface zone with `firewall-cmd --get-active-zones`. Add a source-limited rule to that zone (replace `YOUR_ZONE`):

```sh
firewall-cmd --permanent --zone=YOUR_ZONE --add-rich-rule='rule family="ipv4" source address="172.16.1.132/32" port port="3306" protocol="tcp" accept'
firewall-cmd --reload
```

This adds the web host's access without changing other database consumers. It does not remove pre-existing broad rules; retain your established database firewall policy.

## 2. On web002: PHP and private configuration

Install from your configured Leap 16 repositories:

```sh
zypper install apache2 apache2-mod_php8 php8 php8-cli php8-pdo php8-mysql mariadb-client git rsync util-linux
php -v
php -m | grep -E 'PDO|pdo_mysql|zlib'
```

Keep the PHP handler already used by your other Apache sites. For a mod_php installation, ensure `php8` is enabled (`a2enmod php8`) and the compatible prefork MPM is in use; do not enable mod_php alongside an existing PHP-FPM setup. Also enable `a2enmod ssl`, `a2enmod rewrite`, and `a2enmod env` as needed. The site-local `config.inc.php` works with mod_php and PHP-FPM without an environment variable.

Leap's Apache normally runs as `wwwrun` with group `www`; check your host's existing settings and use that group in these commands:

```sh
install -d -m 750 -o root -g www /etc/lineclash
# Copy only the database CA certificate here, never its private key:
install -m 640 -o root -g www /path/to/database-ca.pem /etc/lineclash/mariadb-ca.pem
```

Create the website directory first (`install -d -m 750 -o wwwrun -g www /srv/www/vhosts/lineclash.kladhest.se`). Then create `/srv/www/vhosts/lineclash.kladhest.se/config.inc.php` using the password created on dbs001:

```php
<?php
return [
    'dsn' => 'mysql:host=sth-ts-dbs001.kladhest.se;port=3306;dbname=lineclash;charset=utf8mb4',
    'user' => 'lineclash_web',
    'password' => 'REPLACE_WITH_THE_DATABASE_PASSWORD',
    'ssl_ca' => '/etc/lineclash/mariadb-ca.pem',
    'ttl' => 180,
];
```

```sh
chown root:www /srv/www/vhosts/lineclash.kladhest.se/config.inc.php
chmod 640 /srv/www/vhosts/lineclash.kladhest.se/config.inc.php
```

The PDO connection verifies the database certificate against that CA and its DNS hostname. Test from **web002**, with an interactive password prompt:

```sh
mariadb --host=sth-ts-dbs001.kladhest.se --port=3306 \
  --user=lineclash_web --password --database=lineclash \
  --ssl-ca=/etc/lineclash/mariadb-ca.pem --ssl-verify-server-cert \
  --execute="SHOW SESSION STATUS LIKE 'Ssl_cipher';"
```

Expect a non-empty cipher. An access-denied error normally means the password or allowed source IP is wrong; a TLS error means the certificate name, CA or server TLS setup needs correcting.

## 3. Refresh website files

The new script is `scripts/refresh-lineclash.sh`; it pulls **kladhest-se/lineclash**, branch **main**, and deploys **public-web/**. It supports initial deployment. If `config.inc.php` is missing, a real refresh copies `config.example.php` into place with mode 640; edit its database settings before running migrations. Dry runs only report the proposed copy. Source PHP is linted before live files change; local config/data/uploads/downloads are preserved, and concurrent refreshes are locked out.

These local project changes must first be published to that repository/branch before the remote refresh can fetch them. Do not expect unpublished local changes to appear through a Git clone.

Copy the script to web002 (for example from your checkout), then run:

```sh
install -m 750 scripts/refresh-lineclash.sh /srv/www/vhosts/refresh-lineclash.sh
LINECLASH_OWNER=wwwrun:www /srv/www/vhosts/refresh-lineclash.sh --dry-run
LINECLASH_OWNER=wwwrun:www /srv/www/vhosts/refresh-lineclash.sh
```

Your OpenFP script used `wwwrun:wwwrun`; this script keeps that default. Set `LINECLASH_OWNER=wwwrun:www` as above if web002 uses standard Leap ownership. The site-local config and external CA are not overwritten. Each real refresh also creates a root-private copy of `config.inc.php` in `/srv/www/vhosts/lineclash.kladhest.se.private-backups/`; dry runs create no backup. The backup directory is mode 700 and its files are mode 600. The script prints the migration command but does not execute it automatically. Existing configs are never replaced by sample values.

Install/update the schema:

```sh
php /srv/www/vhosts/lineclash.kladhest.se/bin/manage.php migrate
```

## 4. Apache virtual host

Set DNS for **lineclash.kladhest.se** to the appropriate web002 address (or the public reverse proxy/NAT address serving it). Install a certificate covering this hostname. Use your existing certificate process.

Copy/adapt `examples/deployment/lineclash-apache.conf` into `/etc/apache2/vhosts.d/lineclash.conf`. Its certificate paths are examples; replace them with your installed full chain and key before enabling the vhost. The document root must be exactly the deployed website directory, never the entire repository. The config denies web access to administration/schema/config files independently of `.htaccess`.

```sh
apache2ctl configtest
systemctl reload apache2
curl --fail https://lineclash.kladhest.se/api/?action=statistics
```

The first API response should be JSON with zero games and live servers. Visit `https://lineclash.kladhest.se/directory/`. Use the TLS virtual host or existing HTTPS reverse proxy, not the development PHP server. If Apache is confined by AppArmor, allow read access to `/srv/www/vhosts/lineclash.kladhest.se/config.inc.php` and its CA and permit its established database/network access; keep the profile enabled.

On SELinux-enabled Leap 16 installations, check `getenforce`. Allow Apache's database connection and label the custom paths using persistent policy rules:

```sh
getsebool httpd_can_network_connect_db
setsebool -P httpd_can_network_connect_db on
semanage fcontext -a -t httpd_sys_content_t '/srv/www/vhosts/lineclash\.kladhest\.se(/.*)?'
semanage fcontext -a -t httpd_config_t '/etc/lineclash(/.*)?'
restorecon -Rv /srv/www/vhosts/lineclash.kladhest.se /etc/lineclash
```

Use `semanage fcontext -m` instead of `-a` if a rule already exists. These are read-only website/config labels; the public PHP service needs no writable document-root directories. Reapply `restorecon` after refreshes if needed. Do not disable SELinux to get the site working. If your upgraded host kept AppArmor instead, use its existing profile and allow the new config/CA paths there.

The website loads `config.inc.php` automatically, including from CLI migration commands. Remove an old Apache `SetEnv LINECLASH_WEB_CONFIG` or PHP-FPM `env[LINECLASH_WEB_CONFIG]` pointing to `/etc/lineclash/web.php`, then reload the relevant service. An explicitly set override still takes precedence.

If your configuration already exists at `/etc/lineclash/web.php`, move it once (without overwriting an existing destination):

```sh
mv -n /etc/lineclash/web.php /srv/www/vhosts/lineclash.kladhest.se/config.inc.php
chown root:www /srv/www/vhosts/lineclash.kladhest.se/config.inc.php
chmod 640 /srv/www/vhosts/lineclash.kladhest.se/config.inc.php
```

The supplied Apache vhost and `.htaccess` deny HTTP access to this file and common backup filenames. Confirm `curl -I https://lineclash.kladhest.se/config.inc.php` returns **403**, not a download.

## 5. Register mgt001 and start the game service

On **web002**, create the operator-approved listing:

```sh
php /srv/www/vhosts/lineclash.kladhest.se/bin/manage.php \
  server main sth-ts-mgt001.kladhest.se 31457 'Kladhest LineClash'
```

Save the printed token privately on mgt001 as `LINECLASH_DIRECTORY_TOKEN`. Re-running this command rotates the token. The listing hostname above resolves to 172.16.1.100 and is reachable only from networks that can route to it. For Internet players, provision a reachable public game hostname and forward TCP 31457 to mgt001 instead.

On **mgt001**, install/build the dedicated server, set `db_file=/var/lib/lineclashd/database.sqlite3` in `/etc/lineclashd/server.cfg`, enable its status API locally if you want live counts, and run:

```sh
python3 /opt/lineclash/scripts/server_directory.py \
  --url https://lineclash.kladhest.se/api/ --server-id main \
  --database /var/lib/lineclashd/database.sqlite3 \
  --state /var/lib/lineclashd/directory.sqlite3 \
  --status-url http://127.0.0.1:31458/status \
  -- /opt/lineclash/server/lineclashd --config /etc/lineclashd/server.cfg
```

Provide the token in the service's private environment file, not on the command line. The service user needs write access to `/var/lib/lineclashd`; mgt001 only needs outbound HTTPS to the website and inbound game TCP 31457. Keep the local status API private. Use a service manager to supervise this wrapper; do not start a second game daemon against the same SQLite database.

The server should appear within one announcement interval (normally 60 seconds). View its private health report with `python3 /opt/lineclash/scripts/server_directory.py --show-health /var/lib/lineclashd/directory.sqlite3.health.json`.

For native clients, set `LINECLASH_DIRECTORY_API=https://lineclash.kladhest.se/api/` before launching LineClash and select **Find servers**.

## 6. Runtime database privileges and future updates

After migration/provisioning succeeds, reduce the website account on **dbs001**:

```sql
REVOKE CREATE, ALTER, INDEX, REFERENCES ON lineclash.* FROM 'lineclash_web'@'172.16.1.132';
```

Runtime uses SELECT/INSERT/UPDATE. Before future schema upgrades, temporarily restore the installation grants from step 1 (or use a separate migration account), run the refresh and explicit migration, and then revoke schema privileges again. Back up the MariaDB database and site-local config.inc.php before upgrading. Future refreshes use the same two script commands; Apache PHP opcache may require a reload to notice replaced files.

## Sources

- [Official Leap 16 package repository](https://download.opensuse.org/distribution/leap/16.0/repo/oss/x86_64/).
- [openSUSE Apache administration reference](https://doc.opensuse.org/documentation/leap/reference/html/book-reference/cha-apache2.html) (the published reference is labelled Leap 15.6; verify local Leap 16 package/module configuration on your host).
- [MariaDB client/server TLS configuration and account requirements](https://mariadb.com/docs/server/security/encryption/data-in-transit-encryption/securing-connections-for-client-and-server).

- [SUSE 16 SELinux file context and web-content guidance](https://documentation.suse.com/sles/16.0/html/SLES-SELinux/index.html).
