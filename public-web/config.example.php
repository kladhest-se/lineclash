<?php
// Copy to config.inc.php beside index.php. HTTP access must be denied by the web server.
// LINECLASH_WEB_CONFIG is an optional override, useful for isolated tests.
return ['dsn'=>'mysql:host=127.0.0.1;dbname=lineclash;charset=utf8mb4',
        'user'=>'lineclash', 'password'=>'CHANGE-ME', 'ttl'=>180,
        // Minimum characters for new/reset central passwords (1-72; default 12).
        'account_password_min_length'=>12,
        // Exact proxy IPs allowed to supply X-Forwarded-For; empty for direct access.
        'trusted_proxies'=>[],
        // Optional: only these exact LAN sources share the website egress public IPv4.
        // 'local_public'=>['from'=>['192.0.2.40'],'address'=>'auto'],
        // For a remote MariaDB host: CA certificate matching its server certificate.
        // 'ssl_ca'=>'/etc/lineclash/mariadb-ca.pem'
];
