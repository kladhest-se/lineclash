<?php
declare(strict_types=1);
function pin_route_redirect(): never {
    $code=is_string($_GET['code']??null)?strtoupper($_GET['code']):'';
    if(!preg_match('/^[A-F0-9]{8}$/D',$code))$code='';
    header('X-Robots-Tag: noindex, nofollow');header('Cache-Control: no-store');header('Referrer-Policy: no-referrer');
    // A root-relative Location retains the public browser origin behind HAProxy.
    header('Location: /account/pin/'.($code!==''?'?code='.rawurlencode($code):''),true,303);exit;
}
