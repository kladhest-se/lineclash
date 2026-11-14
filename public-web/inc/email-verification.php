<?php
// SPDX-License-Identifier: MIT
declare(strict_types=1);
require_once __DIR__.'/accounts.php';
require_once __DIR__.'/smtp.php';
function account_verification_send(PDO $db,string $id,string $email): void {
    $settings=account_mail_settings();
    if(!$settings)throw new InvalidArgumentException('Email verification is required but email delivery is not configured. Try again later.');
    $token=bin2hex(random_bytes(32));$hash=hash('sha256',$token);
    $q=$db->prepare('INSERT INTO account_email_verifications(account_id,token_hash,email,expires) VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE token_hash=VALUES(token_hash),email=VALUES(email),expires=VALUES(expires)');
    $q->execute([$id,$hash,$email,time()+86400]);
    try{
        smtp_send($settings['smtp'],$email,'Activate your LineClash account',"Confirm your email address to activate your LineClash account:\n\n".$settings['url'].'/account/verify/#'.$token."\n\nThis link expires in 24 hours and can be used once. Open the link and choose Verify email, then sign in to the game.\n\nConfirm only if you created this account. If you did not create it, ignore this message. No account access is granted by ignoring it.\n");
    }catch(Throwable $e){
        $db->prepare('DELETE FROM account_email_verifications WHERE account_id=? AND token_hash=?')->execute([$id,$hash]);
        error_log('LineClash activation email: '.$e->getMessage());
        throw new InvalidArgumentException('Activation email could not be sent. Try again later.');
    }
}
function account_verification_resend(PDO $db,string $email): void {
    if(!account_email_verification_required())throw new InvalidArgumentException('Email activation is not required on this website. You can sign in directly.');
    if(!account_mail_settings())throw new InvalidArgumentException('Activation email is temporarily unavailable. Try again later.');
    account_limit($db,'verification-resend',5);$email=account_email($email);account_limit_identity($db,'verification-email',$email,1);
    $q=$db->prepare('SELECT id FROM accounts WHERE email=? AND active=1 AND email_pending=1');$q->execute([$email]);$id=$q->fetchColumn();
    if(!$id)return;
    try{account_verification_send($db,$id,$email);}catch(InvalidArgumentException $e){/* Keep the same public acknowledgement for unknown accounts and mail failures. */}
}
function account_verification_confirm(PDO $db,string $token): void {
    account_limit($db,'verification-confirm',10);
    if(!preg_match('/^[a-f0-9]{64}$/D',$token))throw new InvalidArgumentException('This activation link is invalid or expired. Request a new email.');
    $db->beginTransaction();
    try{
        $q=$db->prepare('SELECT v.account_id FROM account_email_verifications v JOIN accounts a ON a.id=v.account_id WHERE v.token_hash=? AND v.expires>=? AND a.email=v.email AND a.active=1 AND a.email_pending=1 FOR UPDATE');
        $q->execute([hash('sha256',$token),time()]);$id=$q->fetchColumn();
        if(!$id)throw new InvalidArgumentException('This activation link is invalid or expired. Request a new email.');
        // Never change active: an administrator-disabled account cannot be re-enabled by email.
        // Revoke any sessions that were issued while the verification switch was temporarily off.
        $db->prepare('UPDATE accounts SET email_pending=0,auth_version=auth_version+1 WHERE id=?')->execute([$id]);
        $db->prepare('DELETE FROM account_email_verifications WHERE account_id=?')->execute([$id]);
        $db->commit();
    }catch(Throwable $e){if($db->inTransaction())$db->rollBack();throw $e;}
}
