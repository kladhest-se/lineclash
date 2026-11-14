<?php
// SPDX-License-Identifier: MIT
declare(strict_types=1);
require_once __DIR__.'/accounts.php';
require_once __DIR__.'/smtp.php';
function account_recovery_settings(): ?array {
    $path=web_config_path();if(!is_file($path))return null;
    $config=require $path;if(!is_array($config))return null;$smtp=smtp_settings($config);
    $url=$config['public_url']??'https://lineclash.kladhest.se';
    if(!$smtp || !is_string($url) || strlen($url)>512 || !filter_var($url,FILTER_VALIDATE_URL))return null;
    $parts=parse_url($url);
    if(($parts['scheme']??'')!=='https' || isset($parts['user']) || isset($parts['pass']) || isset($parts['query']) || isset($parts['fragment']))return null;
    return ['smtp'=>$smtp,'url'=>rtrim($url,'/')];
}
function account_recovery_request(PDO $db,string $email): void {
    $settings=account_recovery_settings();if(!$settings)throw new InvalidArgumentException('Email password recovery is disabled because SMTP is not configured.');
    account_limit($db,'password-reset',5);
    $email=account_email($email);
    account_limit_identity($db,'password-reset-email',$email,1);
    $q=$db->prepare('SELECT id,auth_version FROM accounts WHERE email=? AND active=1');$q->execute([$email]);$user=$q->fetch();
    if(!$user)return; // The browser always returns the same acknowledgement.
    $token=bin2hex(random_bytes(32));$hash=hash('sha256',$token);
    $q=$db->prepare('INSERT INTO account_password_resets(account_id,token_hash,auth_version,expires) VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE token_hash=VALUES(token_hash),auth_version=VALUES(auth_version),expires=VALUES(expires)');
    $q->execute([$user['id'],$hash,$user['auth_version'],time()+1800]);
    try{
        // Fragment keeps the bearer secret out of web access logs and referrers.
        $url=$settings['url'].'/account/recover/#'.$token;
        smtp_send($settings['smtp'],$email,'Reset your LineClash password',"Someone requested a password reset for your LineClash account.\n\nOpen this link to choose a new password:\n".$url."\n\nThis link expires in 30 minutes and can be used once. If you did not request it, ignore this email. Your password has not changed.\n");
    }catch(Throwable $e){
        $q=$db->prepare('DELETE FROM account_password_resets WHERE account_id=? AND token_hash=?');$q->execute([$user['id'],$hash]);
        error_log('LineClash password recovery: '.$e->getMessage());
        // Do not disclose whether the email exists, even on delivery failure.
    }
}
function account_recovery_reset(PDO $db,string $token,string $password): void {
    if(!account_recovery_settings())throw new InvalidArgumentException('Email password recovery is disabled because SMTP is not configured.');
    account_limit($db,'password-reset-submit',10);account_password($password);
    if(!preg_match('/^[a-f0-9]{64}$/D',$token))throw new InvalidArgumentException('This reset link is invalid or expired. Request a new one.');
    $passwordHash=password_hash($password,PASSWORD_DEFAULT);
    $db->beginTransaction();
    try{
        $q=$db->prepare('SELECT r.account_id,r.auth_version FROM account_password_resets r JOIN accounts a ON a.id=r.account_id WHERE r.token_hash=? AND r.expires>=? AND a.active=1 AND r.auth_version=a.auth_version FOR UPDATE');$q->execute([hash('sha256',$token),time()]);$reset=$q->fetch();
        if(!$reset)throw new InvalidArgumentException('This reset link is invalid or expired. Request a new one.');
        $q=$db->prepare("UPDATE accounts SET password_hash=?,auth_version=auth_version+1,recovery_hash='' WHERE id=?");$q->execute([$passwordHash,$reset['account_id']]);
        $q=$db->prepare('UPDATE account_sessions SET revoked=1 WHERE account_id=?');$q->execute([$reset['account_id']]);
        $q=$db->prepare("UPDATE account_devices SET state='used' WHERE account_id=?");$q->execute([$reset['account_id']]);
        $q=$db->prepare('DELETE FROM account_password_resets WHERE account_id=?');$q->execute([$reset['account_id']]);
        $db->commit();
    }catch(Throwable $e){if($db->inTransaction())$db->rollBack();throw $e;}
}
