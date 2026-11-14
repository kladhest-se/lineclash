<?php
require_once __DIR__.'/../../inc/account-layout.php';
$siteRoot=!empty($accountModePage)?'../':'../../';$accountRoot=!empty($accountModePage)?'./':'../';
require_once __DIR__.'/../../inc/email-verification.php';require_once __DIR__.'/../../inc/site.php';
account_session();header('Cache-Control: no-store');header('Referrer-Policy: no-referrer');
$enabled=account_email_verification_required();$error='';$notice='';$confirm=false;
try{
    if($_SERVER['REQUEST_METHOD']==='POST'){
        account_csrf();$action=$_POST['action']??'';
        if($action==='resend'){
            account_verification_resend(database(),is_string($_POST['email']??null)?$_POST['email']:'');
            $notice='If an account is waiting for activation at that email address, an activation link will be sent. Check your inbox and spam folder.';
        }elseif($action==='confirm'){
            $confirm=true;$user=account_verification_confirm(database(),is_string($_POST['token']??null)?$_POST['token']:'');
            $_SESSION=[];session_regenerate_id(true);$_SESSION['csrf']=bin2hex(random_bytes(32));
            $_SESSION['account_id']=$user['id'];$_SESSION['auth_version']=$user['auth_version'];
            $_SESSION['account_notice']='Email verified. You are now signed in.';
            header('Location: '.$accountRoot,true,303);exit;
        }else throw new InvalidArgumentException('Invalid activation request.');
    }
}catch(InvalidArgumentException $e){$error=$e->getMessage();}
catch(Throwable $e){error_log('LineClash email activation: '.$e->getMessage());http_response_code(503);$error='Email activation is temporarily unavailable. Try again later.';}
site_header('Verify email','account',$siteRoot);
?>
<main id="main" class="community-shell account-auth"><p class="eyebrow">LineClash account</p><h1>Activate your account</h1><?php account_intro();?>
<?php if($error):?><p class="account-error" role="alert"><?=site_h($error)?></p><?php endif;?>
<?php if($notice):?><p class="account-notice" role="status"><?=site_h($notice)?></p><?php endif;?>
<?php if(!$notice || $error):?>
<section id="verification-request" class="community-section"<?=$confirm?' hidden':''?>>
<?php if($enabled):?><p>Enter your registered email address to resend the activation link. A new email replaces your previous link.</p>
<form method="post" class="account-form"><input type="hidden" name="csrf" value="<?=site_h($_SESSION['csrf'])?>"><input type="hidden" name="action" value="resend">
<label for="activation-email">Email</label><input id="activation-email" name="email" type="email" required maxlength="254" autocomplete="email"><button>Resend activation email</button></form>
<?php else:?><p>Email activation is not required on this website. You can sign in directly.</p><?php endif;?></section>
<section id="verification-confirm" class="community-section"<?=$confirm?'':' hidden'?>>
<p>This link verifies your email and signs you in to your LineClash website account.</p>
<form id="verification-form" method="post" class="account-form"><input type="hidden" name="csrf" value="<?=site_h($_SESSION['csrf'])?>"><input type="hidden" name="action" value="confirm"><input id="verification-token" type="hidden" name="token" value="<?=site_h(is_string($_POST['token']??null)?$_POST['token']:'')?>"><button id="verify-email">Verify email and sign in</button></form></section>
<noscript><p>Enable JavaScript to open the private activation link from your email.</p></noscript>
<?php endif;?><?php account_navigation($accountRoot);?></main>
<script src="<?=site_h($siteRoot)?>assets/email-verification.js" defer></script>
<?php site_footer($siteRoot);?>
