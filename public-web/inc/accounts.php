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
function account_password_min_length(): int {
    $config=require web_config_path();$minimum=$config['account_password_min_length']??12;
    return is_int($minimum) && $minimum>=1 && $minimum<=72?$minimum:12;
}
function account_password_hint(): string {
    $minimum=account_password_min_length();return 'Use at least '.$minimum.' '.($minimum===1?'character':'characters').'.';
}
function account_password(string $password): void {
    if(strlen($password)>72)throw new InvalidArgumentException('Password is too long. Use no more than 72 bytes; some characters use more than one byte.');
    $length=preg_match_all('/./us',$password);
    if($length===false || $length<account_password_min_length())throw new InvalidArgumentException('Password is too short. '.account_password_hint());
}

function account_limit(PDO $db,string $scope,int $max=15): void {
    account_limit_identity($db,$scope,directory_peer($_SERVER,require web_config_path()),$max);
}
function account_limit_identity(PDO $db,string $scope,string $identity,int $max): void {
    $source=hash('sha256',$scope.':'.$identity);$now=time();
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
        return !empty($a['direct'])?'central-challenge '.$request:'central-code '.$device['code'];
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

function account_client_api(PDO $db,array $a): string {
    $action=$a['action']??'';
    if($action==='account-client-login') {
        account_limit($db,'account-client-login');
        $handle=account_handle(is_string($a['handle']??null)?$a['handle']:'');
        account_limit_identity($db,'account-client-user',$handle,30);
        $password=is_string($a['password']??null)?$a['password']:'';
        $q=$db->prepare('SELECT id,handle,password_hash,auth_version FROM accounts WHERE handle=? AND active=1');$q->execute([$handle]);$user=$q->fetch();
        $hash=$user['password_hash']??'$2y$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2uheWG/igi.';
        $valid=strlen($password)<=72 && !str_contains($password,"\0") && password_verify($password,$hash);
        if(!$user || !$valid){http_response_code(401);return 'central-error Incorrect username or password';}
        $token=bin2hex(random_bytes(32));$expires=time()+43200;
        $q=$db->prepare('INSERT INTO account_sessions(token_hash,account_id,auth_version,expires) VALUES(?,?,?,?)');$q->execute([hash('sha256',$token),$user['id'],$user['auth_version'],$expires]);
        return 'central-session '.$token.' '.$user['handle'].' '.$expires;
    }
    $token=text_value($a,'session',64);
    if(!preg_match('/^[a-f0-9]{64}$/D',$token)){http_response_code(401);return 'central-expired Sign in to your account again';}
    $q=$db->prepare('SELECT a.id,a.handle FROM account_sessions s JOIN accounts a ON a.id=s.account_id WHERE s.token_hash=? AND s.revoked=0 AND s.expires>=? AND a.active=1 AND s.auth_version=a.auth_version');$q->execute([hash('sha256',$token),time()]);$user=$q->fetch();
    if(!$user){http_response_code(401);return 'central-expired Sign in to your account again';}
    if($action==='account-client-logout'){
        $q=$db->prepare('UPDATE account_sessions SET revoked=1 WHERE token_hash=?');$q->execute([hash('sha256',$token)]);return 'central-signed-out';
    }
    if($action!=='account-client-join')throw new InvalidArgumentException('Unknown account action');
    account_limit($db,'account-client-join',40);
    $challenge=text_value($a,'challenge',64);$host=text_value($a,'host',127);$port=filter_var($a['port']??0,FILTER_VALIDATE_INT);
    if(!preg_match('/^[a-f0-9]{64}$/D',$challenge))throw new InvalidArgumentException('Invalid server challenge');
    $q=$db->prepare("SELECT d.request_hash,s.host,s.port FROM account_devices d JOIN directory_servers s ON s.id=d.server_id WHERE d.request_hash=? AND d.state='pending' AND d.expires>=? AND s.last_seen>=?");$q->execute([$challenge,time(),time()-ttl()]);$device=$q->fetch();
    if(!$device || strcasecmp($device['host'],$host)!==0 || (int)$device['port']!==$port){http_response_code(403);return 'central-error Server verification failed; refresh the public server list';}
    $q=$db->prepare("UPDATE account_devices SET account_id=?,state='approved' WHERE request_hash=? AND state='pending' AND expires>=?");$q->execute([$user['id'],$challenge,time()]);
    if($q->rowCount()!==1){http_response_code(409);return 'central-error Server sign-in expired; reconnect';}
    return 'central-joined '.$user['handle'];
}
