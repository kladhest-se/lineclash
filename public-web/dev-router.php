<?php
$path=parse_url($_SERVER['REQUEST_URI'],PHP_URL_PATH);
if(preg_match('~^/(inc|bin)(/|$)|^/\.?config[^/]*\.php[^/]*$~i',rawurldecode($path))) { http_response_code(404); return true; }
// Match the internal Apache rewrites for PIN addresses without a trailing slash.
if(in_array($path,['/pin','/account/pin'],true)){require __DIR__.$path.'/index.php';return true;}
return false;
