<?php
$path=parse_url($_SERVER['REQUEST_URI'],PHP_URL_PATH);
if(preg_match('~^/(inc|bin)(/|$)|^/\.?config[^/]*\.php[^/]*$~i',rawurldecode($path))) { http_response_code(404); return true; }
return false;
