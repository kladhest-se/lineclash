<?php
require_once __DIR__.'/../../inc/account-layout.php';
$siteRoot=!empty($accountModePage)?'../':'../../';$accountRoot=!empty($accountModePage)?'./':'../';
require_once __DIR__.'/../../inc/account-profile.php';require_once __DIR__.'/../../inc/site.php';
account_session();header('Cache-Control: no-store');header('Referrer-Policy: no-referrer');$error='';$done=false;
try{
    if($_SERVER['REQUEST_METHOD']==='POST'){
        account_csrf();account_profile_confirm_email(database(),is_string($_POST['token']??null)?$_POST['token']:'');
        $_SESSION=[];session_regenerate_id(true);$_SESSION['csrf']=bin2hex(random_bytes(32));$done=true;
    }
}catch(InvalidArgumentException $e){$error=$e->getMessage();}catch(Throwable $e){error_log('LineClash email change: '.$e->getMessage());$error='Account service unavailable. Try again later.';}
site_header('Confirm email change','account',$siteRoot);
?>
<main id="main" class="community-shell account-auth"><p class="eyebrow">LineClash account</p><h1>Confirm email change</h1><?php account_intro();?>
<?php if($error):?><p class="account-error" role="alert"><?=site_h($error)?></p><?php endif;?>
<?php if($done):?><p role="status">Email changed. Sign in with your new email or username.</p>
<?php else:?><p>Your current email stays active until you confirm. Confirm only if you requested this change.</p>
<form method="post" class="account-form"><input type="hidden" name="csrf" value="<?=site_h($_SESSION['csrf'])?>"><input id="email-change-token" type="hidden" name="token" value="<?=site_h(is_string($_POST['token']??null)?$_POST['token']:'')?>"><button id="confirm-email-change">Confirm email change</button></form><noscript><p>Enable JavaScript to open the private email change link.</p></noscript><?php endif;?><?php account_navigation($accountRoot);?></main>
<script src="<?=site_h($siteRoot)?>assets/email-change.js" defer></script><?php site_footer($siteRoot);?>
