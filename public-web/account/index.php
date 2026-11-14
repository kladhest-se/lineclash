<?php
// Account modes share the same address, layout and persistent navigation.
$mode=is_string($_GET['mode']??null)?$_GET['mode']:'';
if(in_array($mode,['recover','activation'],true)){
    $accountModePage=true;require __DIR__.($mode==='recover'?'/recover/index.php':'/verify/index.php');exit;
}
require_once __DIR__.'/../inc/account-layout.php';
require_once __DIR__.'/../inc/accounts.php';require_once __DIR__.'/../inc/account-profile.php';require_once __DIR__.'/../inc/password-recovery.php';require_once __DIR__.'/../inc/site.php';account_session();
$error='';$notice=is_string($_SESSION['account_notice']??null)?$_SESSION['account_notice']:'';unset($_SESSION['account_notice']);$user=null;$code=strtoupper(trim((string)($_POST['code']??$_GET['code']??'')));
if($_SERVER['REQUEST_METHOD']==='GET' && preg_match('/^[A-F0-9]{8}$/D',$code)){header('Location: ../pin/?code='.rawurlencode($code),true,303);exit;}
try {
    $db=database();$user=account_current($db);
    if($_SERVER['REQUEST_METHOD']==='POST'){
        account_csrf();$action=$_POST['action']??'';
        if(in_array($action,['password','email','delete'],true)){
            if(!$user)throw new InvalidArgumentException('Sign in first.');
            $current=is_string($_POST['current_password']??null)?$_POST['current_password']:'';
            if($action==='password'){
                account_profile_password($db,$user,$current,is_string($_POST['new_password']??null)?$_POST['new_password']:'',is_string($_POST['confirm_password']??null)?$_POST['confirm_password']:'');
                $_SESSION['auth_version']++;session_regenerate_id(true);$notice='Password changed. Other sign-ins have been revoked.';
            }elseif($action==='email'){
                $pending=account_profile_email($db,$user,$current,is_string($_POST['new_email']??null)?$_POST['new_email']:'');
                if(!$pending){$_SESSION['auth_version']++;session_regenerate_id(true);}
                $notice=$pending?'Check the new email address to confirm this change. Your current email remains active.':'Email changed. Other sign-ins have been revoked.';
            }else{
                account_profile_delete($db,$user,$current,is_string($_POST['confirmation']??null)?$_POST['confirmation']:'');
                $_SESSION=[];session_regenerate_id(true);$_SESSION['csrf']=bin2hex(random_bytes(32));$user=null;$notice='Account deleted. Personal account data was erased; match results remain with deleted players anonymized.';
            }
        }
        if($action==='logout'){$_SESSION=[];session_destroy();header('Location: ./');exit;}
        if($action==='recover')throw new InvalidArgumentException('Recovery codes are no longer supported. Use the email password recovery page.');
        if(in_array($action,['login','register'],true)){
            account_limit($db,'account-login');$handle=(string)($_POST['handle']??'');$email=(string)($_POST['email']??'');$password=(string)($_POST['password']??'');if($action!=='login')account_password($password);elseif(strlen($password)>72 || str_contains($password,"\0"))throw new InvalidArgumentException('Sign-in details did not match.');
            if($action==='register'){
                $user=account_register($db,$handle,$email,$password);$notice=$user['email_pending']?'Check your email to activate your account, then sign in.':'Account created.';
                if(!$user['email_pending']){session_regenerate_id(true);$_SESSION['account_id']=$user['id'];$_SESSION['auth_version']=1;}
            }else{
                $login=is_string($_POST['login']??null)?$_POST['login']:($email!==''?$email:$handle);
                $candidate=account_lookup($db,$login);
                $valid=$candidate && password_verify($password,$candidate['password_hash']);
                if(!$valid){password_verify($password,'$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.');throw new InvalidArgumentException('Sign-in details did not match.');}
                account_assert_verified($candidate);$user=$candidate;session_regenerate_id(true);$_SESSION['account_id']=$user['id'];$_SESSION['auth_version']=(int)$user['auth_version'];
            }
        }
        if($action==='approve')throw new InvalidArgumentException('Authenticate the client through /pin/.');
    }
    $user=account_current($db);
}catch(InvalidArgumentException $e){$error=$e->getMessage();}catch(Throwable $e){error_log('LineClash account error: '.$e->getMessage());$error='Account service unavailable. Try again later.';}
$register=($_SERVER['REQUEST_METHOD']==='POST'?($_POST['action']??''):($_GET['mode']??$_GET['view']??''))==='register';
if($notice!=='' && !$error)$register=false;
site_header('Account','account');
function account_fields(string $code): void {echo '<input type="hidden" name="csrf" value="'.site_h($_SESSION['csrf']).'"><input type="hidden" name="code" value="'.site_h($code).'">';}
?>
<main id="main" class="community-shell<?=empty($user)?' account-auth':''?>"><p class="eyebrow">LineClash account</p><h1><?=!empty($user)?'@'.site_h($user['handle']):($register?'Create account':'Sign in')?></h1>
<?php if(empty($user))account_intro();?>
<?php if($error):?><p class="account-error" role="alert"><?=site_h($error)?></p><?php endif;?>
<?php if($notice):?><p class="account-notice" role="status"><?=site_h($notice)?></p><?php endif;?>

<?php if(empty($user)):?>
<form method="post" class="account-form" novalidate>
<?php account_fields($code);?>
<?php if($register):?>
<label for="account-handle">Username</label>
<input id="account-handle" name="handle" minlength="3" maxlength="14" autocomplete="username" pattern="[A-Za-z][A-Za-z0-9_]{2,13}" required value="<?=site_h(is_string($_POST['handle']??null)?$_POST['handle']:'')?>">
<label for="account-email">Email</label>
<input id="account-email" name="email" type="email" maxlength="254" autocomplete="email" required value="<?=site_h(is_string($_POST['email']??null)?$_POST['email']:'')?>">
<?php else:?>
<label for="account-login">Username or email</label>
<input id="account-login" name="login" maxlength="254" autocomplete="username" required value="<?=site_h(is_string($_POST['login']??null)?$_POST['login']:'')?>">
<?php endif;?>
<label for="account-password">Password</label>
<input id="account-password" type="password" name="password" required <?=$register?'minlength="'.account_password_min_length().'" ':''?>maxlength="72" autocomplete="<?=$register?'new-password':'current-password'?>">
<button name="action" value="<?=$register?'register':'login'?>"><?=$register?'Create account':'Sign in'?></button>
</form>

<?php else:?>
<p>Your central player identity is <strong>@<?=site_h($user['handle'])?></strong>. Server permissions and bans remain under each server operator's control.</p>
<section class="community-section"><h2>Profile settings</h2>
<p>Username: <strong>@<?=site_h($user['handle'])?></strong></p>
<div class="account-settings"><form method="post" class="account-form"><?php account_fields('');?>
<label for="profile-email">Email</label><input id="profile-email" name="new_email" type="email" value="<?=site_h($user['email']??'')?>" maxlength="254" autocomplete="email" required>
<label for="email-current-password">Current password</label><input id="email-current-password" name="current_password" type="password" maxlength="72" autocomplete="current-password" required><button name="action" value="email">Change email</button></form>
<form method="post" class="account-form"><?php account_fields('');?>
<label for="password-current">Current password</label><input id="password-current" name="current_password" type="password" maxlength="72" autocomplete="current-password" required>
<label for="password-new">New password</label><input id="password-new" name="new_password" type="password" minlength="<?=account_password_min_length()?>" maxlength="72" autocomplete="new-password" required>
<label for="password-confirm">Confirm new password</label><input id="password-confirm" name="confirm_password" type="password" maxlength="72" autocomplete="new-password" required><button name="action" value="password">Change password</button></form></div></section>
<section class="community-section account-danger"><h2>Delete account</h2>
<p>This permanently erases your central account, email, password and sign-in tokens. Your name is removed from this website's statistics and replays. Match scores, placements, team results and wins remain; your entries display as <strong>DELETED PLAYER</strong>.</p>
<p>Independent game-server records, existing downloads and operator backups have separate retention. This cannot be undone.</p>
<details><summary>Delete my account permanently</summary><form method="post" class="account-form"><?php account_fields('');?>
<label for="delete-password">Current password</label><input id="delete-password" name="current_password" type="password" maxlength="72" autocomplete="current-password" required>
<label for="delete-confirmation">Type DELETE to confirm</label><input id="delete-confirmation" name="confirmation" autocomplete="off" pattern="DELETE" required><button name="action" value="delete">Permanently delete account</button></form></details></section>
<form method="post" class="account-form"><?php account_fields('');?><button name="action" value="logout">Sign out</button></form>
<?php endif;account_navigation();?></main><?php site_footer();?>
