<?php
// SPDX-License-Identifier: MIT
require_once __DIR__.'/../../inc/accounts.php';
header('Cache-Control: no-store');header('Referrer-Policy: no-referrer');
if($_SERVER['REQUEST_METHOD']!=='POST'){http_response_code(405);header('Allow: POST');echo 'Use the Sign out button in the top menu.';exit;}
account_session();
try{account_csrf();}catch(InvalidArgumentException $e){http_response_code(403);echo 'This form expired. Reload the page and try again.';exit;}
// Signing out remains available even when the account database is unavailable.
account_logout();header('Location: ../',true,303);exit;
