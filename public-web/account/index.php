<?php
require_once __DIR__.'/../inc/accounts.php';require_once __DIR__.'/../inc/site.php';account_session();
$error='';$notice='';$recovery='';$user=null;$device=null;$code=strtoupper(trim((string)($_POST['code']??$_GET['code']??'')));
try {
    $db=database();account_current($db);
    if($_SERVER['REQUEST_METHOD']==='POST'){
        account_csrf();$action=$_POST['action']??'';
        if($action==='logout'){$_SESSION=[];session_destroy();header('Location: ./');exit;}
        if(in_array($action,['login','register','recover'],true)){
            account_limit($db,'account-login');$handle=account_handle((string)($_POST['handle']??''));$password=(string)($_POST['password']??'');account_password($password);
            if($action==='register'){
                $recovery=bin2hex(random_bytes(24));$id=bin2hex(random_bytes(16));
                $q=$db->prepare('INSERT INTO accounts(id,handle,password_hash,recovery_hash,created_at) VALUES(?,?,?,?,?)');$q->execute([$id,$handle,password_hash($password,PASSWORD_DEFAULT),hash('sha256',$recovery),time()]);session_regenerate_id(true);$_SESSION['account_id']=$id;$_SESSION['auth_version']=1;
            }else{
                $q=$db->prepare('SELECT * FROM accounts WHERE handle=? AND active=1');$q->execute([$handle]);$user=$q->fetch();
                $valid=$user && ($action==='recover'?hash_equals($user['recovery_hash'],hash('sha256',(string)($_POST['recovery']??''))):password_verify($password,$user['password_hash']));
                if(!$valid){password_verify($password,'$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.');throw new InvalidArgumentException('Sign-in details did not match.');}
                if($action==='recover'){$recovery=bin2hex(random_bytes(24));$q=$db->prepare('UPDATE accounts SET password_hash=?,recovery_hash=?,auth_version=auth_version+1 WHERE id=?');$q->execute([password_hash($password,PASSWORD_DEFAULT),hash('sha256',$recovery),$user['id']]);$user['auth_version']++;}
                session_regenerate_id(true);$_SESSION['account_id']=$user['id'];$_SESSION['auth_version']=(int)$user['auth_version'];
            }
        }
        if($action==='approve'){
            account_limit($db,'account-approve');if(empty($_SESSION['account_id']))throw new InvalidArgumentException('Sign in first.');
            $q=$db->prepare("UPDATE account_devices SET account_id=?,state='approved' WHERE code=? AND state='pending' AND expires>=?");$q->execute([$_SESSION['account_id'],$code,time()]);if($q->rowCount()!==1)throw new InvalidArgumentException('That code expired or was already used.');$notice='Approved. Return to the game.';
        }
    }
    $user=account_current($db);
    $device=null;if(preg_match('/^[A-F0-9]{8}$/D',$code)){$q=$db->prepare("SELECT d.code,s.name,s.host,s.port FROM account_devices d JOIN directory_servers s ON s.id=d.server_id WHERE code=? AND state='pending' AND expires>=?");$q->execute([$code,time()]);$device=$q->fetch();}
}catch(InvalidArgumentException $e){$error=$e->getMessage();}catch(Throwable $e){$recovery='';error_log('LineClash account error: '.$e->getMessage());$error='Account service unavailable, or that handle is already registered.';}
site_header('Account','account');
function account_fields(string $code): void {echo '<input type="hidden" name="csrf" value="'.site_h($_SESSION['csrf']).'"><input type="hidden" name="code" value="'.site_h($code).'">';}
?>
<main id="main" class="community-shell"><p class="eyebrow">LineClash account</p><h1><?=!empty($user)?'@'.site_h($user['handle']):'Sign in. Play anywhere.'?></h1>
<?php if($error):?><p class="account-error" role="alert"><?=site_h($error)?></p><?php endif;?>
<?php if($notice):?><p class="account-notice" role="status"><?=site_h($notice)?></p><?php endif;?>
<?php if($recovery):?><section class="community-section"><h2>Save your recovery code</h2><p>This code can reset your password. Store it privately; it is shown only now.</p><code><?=site_h($recovery)?></code></section><?php endif;?>
<?php if(empty($user)):?>
<section class="community-section"><form method="post" class="account-form" novalidate>
<?php account_fields($code);?>
<label for="account-handle">Handle</label>
<input id="account-handle" name="handle" required minlength="3" maxlength="14" autocomplete="username" pattern="[A-Za-z][A-Za-z0-9_]{2,13}" aria-describedby="account-handle-help" value="<?=site_h(is_string($_POST['handle']??null)?$_POST['handle']:'')?>">
<small id="account-handle-help">3–14 letters, numbers or underscores. Start with a letter.</small>
<label for="account-password">Password</label>
<input id="account-password" type="password" name="password" required minlength="12" maxlength="72" autocomplete="current-password" aria-describedby="account-password-help">
<small id="account-password-help">Use at least 12 characters. Maximum 72 bytes.</small>
<button name="action" value="login">Sign in</button><button name="action" value="register">Create account</button>
</form></section>
<details><summary>Recover an account</summary><form method="post" class="account-form" novalidate>
<?php account_fields($code);?>
<label>Handle<input name="handle" required autocomplete="username"></label>
<label>Recovery code<input name="recovery" required autocomplete="off"></label>
<label for="recovery-password">New password</label>
<input id="recovery-password" name="password" type="password" required minlength="12" maxlength="72" autocomplete="new-password" aria-describedby="recovery-password-help">
<small id="recovery-password-help">Use at least 12 characters. Maximum 72 bytes.</small>
<button name="action" value="recover">Reset password</button>
</form></details>
<?php else:?>
<p>Your central player identity is <strong>@<?=site_h($user['handle'])?></strong>. Server permissions and bans remain under each server operator's control.</p>
<?php if($device):?><section class="community-section"><h2>Approve this connection?</h2><p>Server: <strong><?=site_h($device['name'])?></strong> — <?=site_h($device['host'])?>:<?=(int)$device['port']?></p><p>Approve only if this is the code currently shown in your game: <strong><?=site_h($code)?></strong>.</p><form method="post"><?php account_fields($code);?><button name="action" value="approve">Approve sign-in</button></form></section>
<?php else:?><form method="get" class="account-form"><label>Game sign-in code<input name="code" maxlength="8" autocomplete="off" required></label><button>Continue</button></form><?php endif;?>
<form method="post"><?php account_fields('');?><button name="action" value="logout">Sign out</button></form>
<?php endif;?></main><?php site_footer();?>
