<?php
// SPDX-License-Identifier: MIT
declare(strict_types=1);
require_once __DIR__.'/accounts.php';
function account_pin_number(string $input): string {
    $pin=str_replace([' ','-'],'',trim($input));
    if(!preg_match('/^[0-9]{8}$/D',$pin))throw new InvalidArgumentException('Enter the eight-digit PIN shown in your client.');
    return $pin;
}
function account_pin_find(PDO $db,string $pin): array|false {
    $q=$db->prepare("SELECT pin,client,expires FROM account_client_pins WHERE pin=? AND state='pending' AND expires>=?");$q->execute([$pin,time()]);return $q->fetch();
}
function account_pin_decide(PDO $db,string $pin,array $user,bool $approve): void {
    $q=$db->prepare("UPDATE account_client_pins SET account_id=?,auth_version=?,state=? WHERE pin=? AND state='pending' AND expires>=?");
    $q->execute([$user['id'],$user['auth_version'],$approve?'approved':'denied',$pin,time()]);
    if($q->rowCount()!==1)throw new InvalidArgumentException('This PIN expired or was already used. Start again in your client.');
}
function account_pin_api(PDO $db,array $a): string {
    if($a['action']==='account-client-pin-begin'){
        account_limit($db,'client-pin-begin',5);
        $client=$a['client']??'desktop';if(!in_array($client,['desktop','amiga'],true))throw new InvalidArgumentException('Unsupported client type');
        $db->prepare('DELETE FROM account_client_pins WHERE expires<?')->execute([time()]);
        $secret=bin2hex(random_bytes(32));$expires=time()+300;
        for($i=0;$i<5;$i++){
            $pin=str_pad((string)random_int(0,99999999),8,'0',STR_PAD_LEFT);
            try{
                $q=$db->prepare('INSERT INTO account_client_pins(device_hash,pin,client,expires) VALUES(?,?,?,?)');$q->execute([hash('sha256',$secret),$pin,$client,$expires]);
                return 'central-pin '.$secret.' '.$pin.' '.$expires;
            }catch(PDOException $e){if($e->getCode()!=='23000')throw $e;}
        }
        throw new RuntimeException('Unable to allocate a client PIN');
    }
    account_limit($db,'client-pin-poll',600);
    $secret=text_value($a,'device',64);
    if(!preg_match('/^[a-f0-9]{64}$/D',$secret))throw new InvalidArgumentException('Invalid client sign-in request');
    $db->beginTransaction();
    try{
        $hash=hash('sha256',$secret);
        $q=$db->prepare('SELECT * FROM account_client_pins WHERE device_hash=? FOR UPDATE');$q->execute([$hash]);$pin=$q->fetch();
        if(!$pin || (int)$pin['expires']<time() || !in_array($pin['state'],['pending','approved'],true)){
            $db->commit();return $pin && $pin['state']==='denied'?'central-error PIN sign-in declined':'central-error PIN expired or already used; start again';
        }
        if((int)$pin['last_poll']>time()-5){$db->commit();return 'central-slow-down';}
        $db->prepare('UPDATE account_client_pins SET last_poll=? WHERE device_hash=?')->execute([time(),$hash]);
        if($pin['state']==='pending'){$db->commit();return 'central-pending';}
        $q=$db->prepare('SELECT id,handle,email,auth_version FROM accounts WHERE id=? AND active=1 AND auth_version=? FOR UPDATE');$q->execute([$pin['account_id'],$pin['auth_version']]);$user=$q->fetch();
        $db->prepare("UPDATE account_client_pins SET state='used' WHERE device_hash=?")->execute([$hash]);
        if(!$user){$db->commit();return 'central-error Account changed; start PIN sign-in again';}
        $session=account_native_session($db,$user);$db->commit();
        return 'central-session '.$session.' '.($user['email']??'-');
    }catch(Throwable $e){if($db->inTransaction())$db->rollBack();throw $e;}
}
