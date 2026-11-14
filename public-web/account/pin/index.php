<?php
require_once __DIR__.'/../../inc/account-pins.php';require_once __DIR__.'/../../inc/site.php';
account_session();header('Cache-Control: no-store');header('Referrer-Policy: no-referrer');
$error='';$notice='';$pin='';$device=null;$user=null;
try{
    $db=database();$user=account_current($db);
    if($_SERVER['REQUEST_METHOD']==='POST'){
        account_csrf();$action=$_POST['action']??'';
        account_limit($db,'client-pin-browser',15);
        $pin=account_pin_number(is_string($_POST['pin']??null)?$_POST['pin']:'');
        $device=account_pin_find($db,$pin);
        if(!$device)throw new InvalidArgumentException('This PIN expired or was already used. Start again in your client.');
        if($action==='login'){
            account_limit($db,'account-login');
            $login=is_string($_POST['login']??null)?strtolower(trim($_POST['login'])):'';
            if(strlen($login)>254)throw new InvalidArgumentException('Sign-in details did not match.');
            account_limit_identity($db,'account-client-user',$login,30);
            $password=is_string($_POST['password']??null)?$_POST['password']:'';
            $found=account_lookup($db,$login);
            $hash=$found['password_hash']??'$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.';
            if(strlen($password)>72 || str_contains($password,"\0") || !password_verify($password,$hash) || !$found)throw new InvalidArgumentException('Sign-in details did not match.');
            account_assert_verified($found);session_regenerate_id(true);$_SESSION['account_id']=$found['id'];$_SESSION['auth_version']=(int)$found['auth_version'];$user=account_current($db);
        }elseif($action==='approve' || $action==='deny'){
            if(!$user)throw new InvalidArgumentException('Sign in before approving this client.');
            account_pin_decide($db,$pin,$user,$action==='approve');
            $notice=$action==='approve'?'Client approved. Return to LineClash.':'Client sign-in declined.';$device=null;$pin='';
        }elseif($action==='switch'){
            unset($_SESSION['account_id'],$_SESSION['auth_version']);session_regenerate_id(true);$user=null;
        }elseif($action!=='lookup')throw new InvalidArgumentException('Invalid PIN action.');
    }
}catch(InvalidArgumentException $e){$error=$e->getMessage();}
catch(Throwable $e){error_log('LineClash PIN sign-in: '.$e->getMessage());http_response_code(503);$error='PIN sign-in is temporarily unavailable. Try again later.';}
site_header('Sign in with a PIN','account','../../');
function pin_fields(string $pin): void {echo '<input type="hidden" name="csrf" value="'.site_h($_SESSION['csrf']).'"><input type="hidden" name="pin" value="'.site_h($pin).'">';}
?>
<main id="main" class="community-shell"><p class="eyebrow">LineClash account</p><h1>Sign in your client.</h1>
<?php if($error):?><p class="account-error" role="alert"><?=site_h($error)?></p><?php endif;?>
<?php if($notice):?><p class="account-notice" role="status"><?=site_h($notice)?></p><?php endif;?>
<?php if($device):?>
<section class="community-section"><h2><?=site_h($device['client']==='amiga'?'Amiga client':'Desktop client')?></h2><p>PIN: <strong><?=site_h(substr($pin,0,4).' '.substr($pin,4))?></strong></p>
<p>Continue only if this PIN is displayed on the client you are signing in. Never approve a PIN sent to you by someone else.</p>
<?php if(!$user):?>
<form method="post" class="account-form"><?php pin_fields($pin);?>
<label for="pin-login">Username or email</label><input id="pin-login" name="login" maxlength="254" autocomplete="username" required>
<label for="pin-password">Password</label><input id="pin-password" type="password" name="password" maxlength="72" autocomplete="current-password" required>
<button name="action" value="login">Sign in</button></form>
<p><a href="../">Create an account</a> · <a href="../recover/">Forgot password?</a> · <a href="../verify/">Resend activation email</a></p>
<?php else:?>
<p>Sign this client in as <strong>@<?=site_h($user['handle'])?></strong>?</p>
<form method="post" class="account-form"><?php pin_fields($pin);?><button name="action" value="approve">Approve client</button><button name="action" value="deny">Decline</button><button name="action" value="switch">Use another account</button></form>
<?php endif;?></section>
<?php elseif(!$notice):?>
<p>Choose <strong>Sign in with a PIN</strong> in the game, then enter its eight-digit PIN here.</p>
<form method="post" class="account-form"><input type="hidden" name="csrf" value="<?=site_h($_SESSION['csrf'])?>"><input type="hidden" name="action" value="lookup">
<label for="client-pin">Client PIN</label><input id="client-pin" name="pin" inputmode="numeric" autocomplete="off" maxlength="9" placeholder="1234 5678" required><button>Continue</button></form>
<?php endif;?><p><a href="../">Back to account</a></p></main><?php site_footer('../../');?>
