<?php
declare(strict_types=1);
require_once __DIR__.'/service.php';
function account_session(): void {
    if(session_status()===PHP_SESSION_ACTIVE)return;
    session_name('lineclash_account');ini_set('session.use_strict_mode','1');
    session_set_cookie_params(['secure'=>true,'httponly'=>true,'samesite'=>'Lax','path'=>'/']);session_start();
    if(!isset($_SESSION['csrf']))$_SESSION['csrf']=bin2hex(random_bytes(32));
}
function account_current(PDO $db): ?array {
    if(empty($_SESSION['account_id']))return null;
    $q=$db->prepare('SELECT id,handle,created_at,auth_version FROM accounts WHERE id=? AND active=1');$q->execute([$_SESSION['account_id']]);$user=$q->fetch();
    if(!$user || (int)$user['auth_version']!==(int)($_SESSION['auth_version']??0)){unset($_SESSION['account_id'],$_SESSION['auth_version']);return null;}
    return $user;
}
function account_csrf(): void {
    if(!is_string($_POST['csrf']??null)||!hash_equals($_SESSION['csrf'],$_POST['csrf']))throw new InvalidArgumentException('This form expired. Try again.');
}
function account_handle(string $handle): string {
    $handle=strtolower(trim($handle));if(!preg_match('/^[a-z][a-z0-9_]{2,13}$/D',$handle))throw new InvalidArgumentException('Use 3–14 letters, digits or underscores, starting with a letter.');return $handle;
}
function account_password(string $password): void {
    if(strlen($password)<12)throw new InvalidArgumentException('Password is too short. Use at least 12 characters.');
    if(strlen($password)>72)throw new InvalidArgumentException('Password is too long. Use no more than 72 bytes; some characters use more than one byte.');
}
function account_limit(PDO $db,string $scope,int $max=15): void {
    $source=hash('sha256',$scope.':'.directory_peer($_SERVER,require web_config_path()));$now=time();
    $q=$db->prepare('INSERT INTO directory_registration_limits(source_hash,window_start,requests) VALUES(?,?,1) ON DUPLICATE KEY UPDATE requests=IF(window_start<? ,1,requests+1),window_start=IF(window_start<? ,VALUES(window_start),window_start)');$q->execute([$source,$now,$now-60,$now-60]);
    $q=$db->prepare('SELECT requests FROM directory_registration_limits WHERE source_hash=?');$q->execute([$source]);if((int)$q->fetchColumn()>$max)throw new InvalidArgumentException('Too many attempts. Try again in a minute.');
}
function account_api(PDO $db,array $a): string {
    $action=$a['action']??'';$token=text_value($a,'server_key',64);$nonce=text_value($a,'nonce',64);
    if(!preg_match('/^[a-f0-9]{64}$/D',$token)||!preg_match('/^[a-f0-9]{64}$/D',$nonce))throw new InvalidArgumentException('Invalid authentication request');
    $q=$db->prepare('SELECT id,name,host,port FROM directory_servers WHERE token_hash=? AND last_seen>=?');$q->execute([hash('sha256',$token),time()-ttl()]);$server=$q->fetch();
    if(!$server){http_response_code(403);return 'central-error Server must be verified in the directory first';}
    $request=hash('sha256',$nonce);$q=$db->prepare('SELECT * FROM account_devices WHERE request_hash=? AND server_id=?');$q->execute([$request,$server['id']]);$device=$q->fetch();
    if($action==='account-begin'){
        account_limit($db,'account-begin',20);
        if(!$device){$code=strtoupper(bin2hex(random_bytes(4)));$q=$db->prepare('INSERT INTO account_devices(request_hash,code,server_id,expires,state) VALUES(?,?,?,?,?)');$q->execute([$request,$code,$server['id'],time()+300,'pending']);$device=['code'=>$code,'expires'=>time()+300];}
        if((int)$device['expires']<time())return 'central-error Sign-in expired';
        return 'central-code '.$device['code'];
    }
    if(!$device || (int)$device['expires']<time())return 'central-error Sign-in expired';
    if($device['state']==='pending')return 'central-pending';
    if($device['state']!=='approved')return 'central-error Sign-in unavailable';
    $q=$db->prepare('SELECT id,handle FROM accounts WHERE id=? AND active=1');$q->execute([$device['account_id']]);$user=$q->fetch();
    if(!$user)return 'central-error Account unavailable';
    $q=$db->prepare("UPDATE account_devices SET state='used' WHERE request_hash=? AND state='approved'");$q->execute([$request]);
    if($q->rowCount()!==1)return 'central-error Sign-in already used';
    return 'central-approved '.$user['id'].' '.$user['handle'];
}
