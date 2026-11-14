<?php
// Copy to config.inc.php beside index.php. HTTP access must be denied by the web server.
// LINECLASH_WEB_CONFIG is an optional override, useful for isolated tests.
return ['dsn'=>'mysql:host=127.0.0.1;dbname=lineclash;charset=utf8mb4',
        'user'=>'lineclash', 'password'=>'CHANGE-ME', 'ttl'=>180,
        // Minimum characters for new/reset central passwords (1-72; default 12).
        'account_password_min_length'=>12,
        // Canonical HTTPS website URL used in email links (never taken from Host).
        'public_url'=>'https://lineclash.kladhest.se',
        // Optional SMTP. Leave host/from_address empty to disable password recovery.
        'smtp'=>[
            'host'=>'',                    // SMTP relay hostname or IP
            'port'=>587,
            'encryption'=>'starttls',       // starttls, tls (implicit TLS), or none
            'username'=>'', 'password'=>'', // Empty username: no SMTP authentication
            'from_address'=>'',            // e.g. noreply@kladhest.se
            'from_name'=>'LineClash',
            'hello_host'=>'lineclash.kladhest.se', // EHLO identity
            'source_ip'=>'',               // Optional local outgoing interface IP
            'ca_file'=>'',                 // Optional CA file for a private relay CA
            'timeout'=>10,                 // Seconds per connection/read/write (1-30)
        ],
        // Exact proxy IPs allowed to supply X-Forwarded-For; empty for direct access.
        'trusted_proxies'=>[],
        // Optional: only these exact LAN sources share the website egress public IPv4.
        // 'local_public'=>['from'=>['192.0.2.40'],'address'=>'auto'],
        // For a remote MariaDB host: CA certificate matching its server certificate.
        // 'ssl_ca'=>'/etc/lineclash/mariadb-ca.pem'
];
