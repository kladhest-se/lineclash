<?php
// Copy to config.inc.php beside index.php. HTTP access must be denied by the web server.
// LINECLASH_WEB_CONFIG is an optional override, useful for isolated tests.
return ['dsn'=>'mysql:host=127.0.0.1;dbname=lineclash;charset=utf8mb4',
        'user'=>'lineclash', 'password'=>'CHANGE-ME', 'ttl'=>180,
        // For a remote MariaDB host: CA certificate matching its server certificate.
        // 'ssl_ca'=>'/etc/lineclash/mariadb-ca.pem'
];
