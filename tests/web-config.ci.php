<?php
// Disposable CI database only. Production credentials belong in the private, untracked site config.inc.php.
return ['dsn'=>'mysql:host=mariadb;dbname=lineclash_test;charset=utf8mb4',
        'user'=>'root', 'password'=>getenv('MYSQL_ROOT_PASSWORD'), 'ttl'=>180];
