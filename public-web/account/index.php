<?php
require_once __DIR__.'/../inc/accounts.php';require_once __DIR__.'/../inc/password-recovery.php';require_once __DIR__.'/../inc/site.php';account_session();
$error='';$notice='';$user=null;$device=null;$code=strtoupper(trim((string)($_POST['code']??$_GET['code']??'')));
try {
    $db=database();account_current($db);
    if($_SERVER['REQUEST_METHOD']==='POST'){
        account_csrf();$action=$_POST['action']??'';
        if($action==='logout'){$_SESSION=[];session_destroy();header('Location: ./');exit;}
        if($action==='recover')throw new InvalidArgumentException('Recovery codes are no longer supported. Use the email password recovery page.');
        if(in_array($action,['login','register'],true)){
            account_limit($db,'account-login');$handle=(string)($_POST['handle']??'');$email=(string)($_POST['email']??'');$password=(string)($_POST['password']??'');if($action!=='login')account_password($password);elseif(strlen($password)>72 || str_contains($password,"\0"))throw new InvalidArgumentException('Sign-in details did not match.');
            if($action==='register'){
                $user=account_register($db,$handle,$email,$password);$notice=$user['email_pending']?'Check your email to activate your account, then sign in.':'Account created.';
                if(!$user['email_pending']){session_regenerate_id(true);$_SESSION['account_id']=$user['id'];$_SESSION['auth_version']=1;}
            }else{
                $candidate=account_lookup($db,$email!==''?$email:$handle);
                $valid=$candidate && password_verify($password,$candidate['password_hash']);
                if(!$valid){password_verify($password,'$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.');throw new InvalidArgumentException('Sign-in details did not match.');}
                account_assert_verified($candidate);$user=$candidate;session_regenerate_id(true);$_SESSION['account_id']=$user['id'];$_SESSION['auth_version']=(int)$user['auth_version'];
            }
        }
        if($action==='approve'){
            account_limit($db,'account-approve');if(empty($_SESSION['account_id']))throw new InvalidArgumentException('Sign in first.');
            $q=$db->prepare("UPDATE account_devices SET account_id=?,state='approved' WHERE code=? AND state='pending' AND expires>=?");$q->execute([$_SESSION['account_id'],$code,time()]);if($q->rowCount()!==1)throw new InvalidArgumentException('That code expired or was already used.');$notice='Approved. Return to the game.';
        }
    }
    $user=account_current($db);
    $device=null;if(preg_match('/^[A-F0-9]{8}$/D',$code)){$q=$db->prepare("SELECT d.code,s.name,s.host,s.port FROM account_devices d JOIN directory_servers s ON s.id=d.server_id WHERE code=? AND state='pending' AND expires>=?");$q->execute([$code,time()]);$device=$q->fetch();}
}catch(InvalidArgumentException $e){$error=$e->getMessage();}catch(Throwable $e){error_log('LineClash account error: '.$e->getMessage());$error='Account service unavailable, or that handle is already registered.';}
site_header('Account','account');
function account_fields(string $code): void {echo '<input type="hidden" name="csrf" value="'.site_h($_SESSION['csrf']).'"><input type="hidden" name="code" value="'.site_h($code).'">';}
?>
<main id="main" class="community-shell"><p class="eyebrow">LineClash account</p><h1><?=!empty($user)?'@'.site_h($user['handle']):'Sign in. Play anywhere.'?></h1>
<?php if($error):?><p class="account-error" role="alert"><?=site_h($error)?></p><?php endif;?>
<?php if($notice):?><p class="account-notice" role="status"><?=site_h($notice)?></p><?php endif;?>

<?php if(empty($user)):?>
<section class="community-section"><form method="post" class="account-form" novalidate>
<?php account_fields($code);?>
<label for="account-email">Email</label>
<input id="account-email" name="email" type="email" maxlength="254" autocomplete="email" value="<?=site_h(is_string($_POST['email']??null)?$_POST['email']:'')?>">
<label for="account-handle">Username (new accounts)</label>
<input id="account-handle" name="handle" minlength="3" maxlength="14" autocomplete="username" pattern="[A-Za-z][A-Za-z0-9_]{2,13}" value="<?=site_h(is_string($_POST['handle']??null)?$_POST['handle']:'')?>">
<label for="account-password">Password</label>
<input id="account-password" type="password" name="password" required minlength="<?=account_password_min_length()?>" maxlength="72" autocomplete="current-password">
<button name="action" value="login">Sign in</button><button name="action" value="register">Create account</button>
<a href="pin/">Sign in a client with a PIN</a>
<a href="verify/">Resend activation email</a>
</form></section>
<?php if(account_recovery_settings()):?><p><a href="recover/">Forgot password?</a></p><?php else:?><p>Email password recovery is disabled because SMTP is not configured.</p><?php endif;?>
<?php else:?>
<p><a href="pin/">Sign in a client with a PIN</a>
 · <a href="verify/">Resend activation email</a></p>
<p>Your central player identity is <strong>@<?=site_h($user['handle'])?></strong>. Server permissions and bans remain under each server operator's control.</p>
<?php if($device):?><section class="community-section"><h2>Approve this connection?</h2><p>Server: <strong><?=site_h($device['name'])?></strong> — <?=site_h($device['host'])?>:<?=(int)$device['port']?></p><p>Approve only if this is the code currently shown in your game: <strong><?=site_h($code)?></strong>.</p><form method="post"><?php account_fields($code);?><button name="action" value="approve">Approve sign-in</button></form></section>
<?php else:?><form method="get" class="account-form"><label>Game sign-in code<input name="code" maxlength="8" autocomplete="off" required></label><button>Continue</button></form><?php endif;?>
<form method="post"><?php account_fields('');?><button name="action" value="logout">Sign out</button></form>
<?php endif;?></main><?php site_footer();?>
