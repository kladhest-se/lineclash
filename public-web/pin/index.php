<?php
// Compatibility for older clients/bookmarks. Never construct an origin from
// Apache's internal scheme, listener port, Host or forwarded headers.
require_once __DIR__.'/../inc/pin-route.php';
if($_SERVER['REQUEST_METHOD']!=='POST')pin_route_redirect();
// Finish already-open forms locally, without redirecting posted credentials.
require __DIR__.'/../account/pin/index.php';
