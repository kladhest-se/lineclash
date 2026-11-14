<?php
require_once __DIR__.'/../inc/account-pins.php';require_once __DIR__.'/../inc/site.php';
account_session();header('X-Robots-Tag: noindex, nofollow');header('Cache-Control: no-store');header('Referrer-Policy: no-referrer');
$error='';$notice='';$pin='';$device=null;
$prefill=is_string($_GET['code']??null)?strtoupper($_GET['code']):'';
if(!preg_match('/^[A-F0-9]{8}$/D',$prefill))$prefill='';
try{
    if($_SERVER['REQUEST_METHOD']==='POST'){
        account_csrf();account_limit(database(),'client-pin-browser',15);$db=database();
        $pin=is_string($_POST['pin']??null)?strtoupper(str_replace([' ','-'],'',trim($_POST['pin']))):'';
        if(!preg_match('/^[A-F0-9]{8}$/D',$pin))throw new InvalidArgumentException('Enter the PIN shown in your client.');
        $device=ctype_digit($pin)?account_pin_find($db,$pin):false;
        if($device){$device['kind']='client';}
        else{
            // Older clients have a connection-bound game-server code, not a native session PIN.
            $q=$db->prepare("SELECT d.code AS pin,d.expires,s.name,s.host,s.port FROM account_devices d JOIN directory_servers s ON s.id=d.server_id WHERE d.code=? AND d.state='pending' AND d.expires>=? AND s.last_seen>=?");$q->execute([$pin,time(),time()-ttl()]);$device=$q->fetch();
            if($device)$device['kind']='server';
        }
        if(!$device)throw new InvalidArgumentException('This PIN expired or was already used. Start again in your client.');
        $action=$_POST['action']??'';
        if($action==='authenticate'){
            // Always request fresh credentials, even if the browser has an account session.
            account_limit($db,'account-login');
            $login=is_string($_POST['login']??null)?strtolower(trim($_POST['login'])):'';
            if(strlen($login)>254)throw new InvalidArgumentException('Sign-in details did not match.');
            account_limit_identity($db,'account-client-user',$login,30);
            $password=is_string($_POST['password']??null)?$_POST['password']:'';$user=account_lookup($db,$login);
            $hash=$user['password_hash']??'$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.';
            if(strlen($password)>72 || str_contains($password,"\0") || !password_verify($password,$hash) || !$user)throw new InvalidArgumentException('Sign-in details did not match.');
            account_assert_verified($user);
            if($device['kind']==='client')account_pin_decide($db,$pin,$user,true);
            else{
                $q=$db->prepare("UPDATE account_devices SET account_id=?,state='approved' WHERE code=? AND state='pending' AND expires>=?");$q->execute([$user['id'],$pin,time()]);
                if($q->rowCount()!==1)throw new InvalidArgumentException('This PIN expired or was already used.');
            }
            $notice='Client authenticated. Return to LineClash.';$device=null;$pin='';
        }elseif($action!=='lookup')throw new InvalidArgumentException('Enter your PIN, then sign in to authenticate the client.');
    }
}catch(InvalidArgumentException $e){$error=$e->getMessage();}
catch(Throwable $e){error_log('LineClash PIN sign-in: '.$e->getMessage());http_response_code(503);$error='PIN sign-in is temporarily unavailable. Try again later.';}
site_header('Authenticate a client','account','../');
?>
<main id="main" class="community-shell"><p class="eyebrow">LineClash account</p><h1>Authenticate your client.</h1>
<?php if($error):?><p class="account-error" role="alert"><?=site_h($error)?></p><?php endif;?>
<?php if($notice):?><p class="account-notice" role="status"><?=site_h($notice)?></p><?php endif;?>
<?php if($device):?>
<section class="community-section"><h2>Sign in to authenticate</h2><p>PIN: <strong><?=site_h(substr($pin,0,4).' '.substr($pin,4))?></strong></p>
<?php if($device['kind']==='server'):?><p>Server: <strong><?=site_h($device['name'])?></strong> — <?=site_h($device['host'])?>:<?=(int)$device['port']?></p><?php else:?><p><?=site_h($device['client']==='amiga'?'Amiga client':'Desktop client')?></p><?php endif;?>
<p>Continue only if this PIN is displayed on your own client. Never sign in for a PIN sent to you by someone else.</p>
<form method="post" class="account-form"><input type="hidden" name="csrf" value="<?=site_h($_SESSION['csrf'])?>"><input type="hidden" name="pin" value="<?=site_h($pin)?>"><input type="hidden" name="action" value="authenticate">
<label for="pin-login">Username or email</label><input id="pin-login" name="login" maxlength="254" autocomplete="username" required>
<label for="pin-password">Password</label><input id="pin-password" type="password" name="password" maxlength="72" autocomplete="current-password" required>
<button>Sign in and authenticate client</button></form>
<p><a href="../account/">Create an account</a> · <a href="../account/recover/">Forgot password?</a> · <a href="../account/verify/">Resend activation email</a></p>
<p><a href="./">Enter another PIN</a></p></section>
<?php elseif(!$notice):?>
<p>Enter the PIN shown in your LineClash client.</p>
<form method="post" class="account-form"><input type="hidden" name="csrf" value="<?=site_h($_SESSION['csrf'])?>"><input type="hidden" name="action" value="lookup">
<label for="client-pin">Client PIN</label><input id="client-pin" name="pin" autocomplete="off" maxlength="9" placeholder="1234 5678" value="<?=site_h($prefill)?>" required><button>Continue</button></form>
<?php endif;?><p><a href="../account/">Back to account</a></p></main><?php site_footer('../');?>
