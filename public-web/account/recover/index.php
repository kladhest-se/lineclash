<?php
require_once __DIR__.'/../../inc/account-layout.php';
$siteRoot=!empty($accountModePage)?'../':'../../';$accountRoot=!empty($accountModePage)?'./':'../';
require_once __DIR__.'/../../inc/password-recovery.php';require_once __DIR__.'/../../inc/site.php';
account_session();header('Cache-Control: no-store');header('Referrer-Policy: no-referrer');
$enabled=account_recovery_settings()!==null;$error='';$notice='';$reset=false;
try{
    if($_SERVER['REQUEST_METHOD']==='POST'){
        account_csrf();
        if(!$enabled)throw new InvalidArgumentException('Email password recovery is disabled because SMTP is not configured.');
        $action=$_POST['action']??'';
        if($action==='request'){
            account_recovery_request(database(),is_string($_POST['email']??null)?$_POST['email']:'');
            $notice='If an active account uses that email address, a reset link will be sent. Check your inbox and spam folder.';
        }elseif($action==='reset'){
            $reset=true;$password=is_string($_POST['password']??null)?$_POST['password']:'';
            if($password!==($_POST['confirm']??null))throw new InvalidArgumentException('Passwords do not match.');
            account_recovery_reset(database(),is_string($_POST['token']??null)?$_POST['token']:'',$password);
            $_SESSION=[];session_regenerate_id(true);$_SESSION['csrf']=bin2hex(random_bytes(32));
            $notice='Password changed. Sign in with your new password.';$reset=false;
        }else throw new InvalidArgumentException('Invalid recovery request.');
    }
}catch(InvalidArgumentException $e){$error=$e->getMessage();}
catch(Throwable $e){error_log('LineClash recovery service: '.$e->getMessage());$error='Recovery service unavailable. Try again later.';}
site_header('Password recovery','account',$siteRoot);
?>
<main id="main" class="community-shell account-auth"><p class="eyebrow">LineClash account</p><h1>Reset your password</h1><?php account_intro();?>
<?php if($error):?><p class="account-error" role="alert"><?=site_h($error)?></p><?php endif;?>
<?php if($notice):?><p class="account-notice" role="status"><?=site_h($notice)?></p><?php endif;?>
<?php if(!$enabled):?><p>Email password recovery is disabled because SMTP is not configured.</p>
<?php elseif(!$notice || $error):?>
<section id="recovery-request" class="community-section"<?=$reset?' hidden':''?>>
<form method="post" class="account-form"><input type="hidden" name="csrf" value="<?=site_h($_SESSION['csrf'])?>"><input type="hidden" name="action" value="request">
<label for="recovery-email">Email</label><input id="recovery-email" name="email" type="email" required maxlength="254" autocomplete="email"><button>Send reset link</button></form></section>
<section id="recovery-reset" class="community-section"<?=$reset?'':' hidden'?>><p>Choose a new password. <?=site_h(account_password_hint())?> Existing sign-ins will be invalidated.</p>
<form method="post" class="account-form"><input type="hidden" name="csrf" value="<?=site_h($_SESSION['csrf'])?>"><input type="hidden" name="action" value="reset"><input id="reset-token" type="hidden" name="token" value="<?=site_h(is_string($_POST['token']??null)?$_POST['token']:'')?>">
<label for="reset-password">New password</label><input id="reset-password" name="password" type="password" required minlength="<?=account_password_min_length()?>" maxlength="72" autocomplete="new-password">
<label for="reset-confirm">Confirm password</label><input id="reset-confirm" name="confirm" type="password" required maxlength="72" autocomplete="new-password"><button>Change password</button></form></section>
<noscript><p>Enable JavaScript to open the private reset link from your email.</p></noscript>
<?php endif;?>
<?php account_navigation($accountRoot);?></main>
<script src="<?=site_h($siteRoot)?>assets/password-recovery.js" defer></script>
<?php site_footer($siteRoot);?>
