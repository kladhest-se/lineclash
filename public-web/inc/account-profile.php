<?php
// SPDX-License-Identifier: MIT
declare(strict_types=1);
require_once __DIR__.'/accounts.php';
require_once __DIR__.'/account-avatars.php';
require_once __DIR__.'/smtp.php';
require_once __DIR__.'/account-privacy.php';
function account_profile_avatar(PDO $db,array $user,string $avatar): void {
    if(!isset(account_avatars()[$avatar]))throw new InvalidArgumentException('Choose one of the available avatars.');
    account_limit($db,'account-avatar',30);$db->beginTransaction();
    try{
        $q=$db->prepare('SELECT id FROM accounts WHERE id=? AND auth_version=? AND active=1'.account_verification_sql().' FOR UPDATE');$q->execute([$user['id'],$user['auth_version']]);
        if(!$q->fetchColumn())throw new InvalidArgumentException('Sign in again to save your avatar.');
        $db->prepare('UPDATE accounts SET avatar=? WHERE id=?')->execute([$avatar,$user['id']]);$db->commit();
    }catch(Throwable $e){if($db->inTransaction())$db->rollBack();throw $e;}
}
function account_profile_statistics(PDO $db,string $id): array {
    $q=$db->prepare('SELECT COUNT(*) AS matches_played,COALESCE(SUM(won),0) AS wins,COALESCE(SUM(lines_cleared),0) AS lines_cleared,COALESCE(SUM(specials_sent),0) AS specials_sent FROM directory_players WHERE account_id=? AND deleted=0');$q->execute([$id]);return $q->fetch();
}
function account_profile_lock(PDO $db,array $sessionUser,string $password): array {
    $q=$db->prepare('SELECT * FROM accounts WHERE id=? AND auth_version=? AND active=1'.account_verification_sql().' FOR UPDATE');$q->execute([$sessionUser['id'],$sessionUser['auth_version']]);$user=$q->fetch();
    if(!$user || strlen($password)>72 || str_contains($password,"\0") || !password_verify($password,$user['password_hash']))throw new InvalidArgumentException('Enter your current password to confirm this change.');
    return $user;
}
function account_profile_clear_counters(PDO $db,array $identities): void {
    foreach(array_filter($identities) as $identity)foreach(['account-client-user','password-reset-email','verification-email'] as $scope)$db->prepare('DELETE FROM directory_registration_limits WHERE source_hash=?')->execute([hash('sha256',$scope.':'.$identity)]);
}
function account_profile_revoke(PDO $db,string $id): void {
    $db->prepare('UPDATE accounts SET auth_version=auth_version+1 WHERE id=?')->execute([$id]);
    $db->prepare('UPDATE account_sessions SET revoked=1 WHERE account_id=?')->execute([$id]);
    $db->prepare("UPDATE account_devices SET state='used' WHERE account_id=?")->execute([$id]);
    foreach(['account_password_resets','account_email_changes','account_email_verifications'] as $table)$db->prepare('DELETE FROM '.$table.' WHERE account_id=?')->execute([$id]);
}
function account_profile_notice(?string $email,string $subject,string $body): void {
    $settings=account_mail_settings();if(!$settings || !$email)return;
    try{smtp_send($settings['smtp'],$email,$subject,$body);}catch(Throwable $e){error_log('LineClash account notification: '.$e->getMessage());}
}
function account_profile_password(PDO $db,array $sessionUser,string $current,string $password,string $confirm): void {
    if($password!==$confirm)throw new InvalidArgumentException('Passwords do not match.');account_password($password);account_limit($db,'account-settings',10);
    $hash=password_hash($password,PASSWORD_DEFAULT);$db->beginTransaction();
    try{
        $user=account_profile_lock($db,$sessionUser,$current);
        $db->prepare('UPDATE accounts SET password_hash=? WHERE id=?')->execute([$hash,$user['id']]);account_profile_revoke($db,$user['id']);$db->commit();
    }catch(Throwable $e){if($db->inTransaction())$db->rollBack();throw $e;}
    account_profile_notice($user['email'],'LineClash password changed',"Your LineClash password was changed. If you did not make this change, recover your account through the official website.\n");
}
function account_profile_email(PDO $db,array $sessionUser,string $current,string $email): bool {
    $email=account_email($email);account_limit($db,'account-settings',10);$verify=account_email_verification_required();
    $settings=account_mail_settings();if($verify && !$settings)throw new InvalidArgumentException('Email verification is required but email delivery is unavailable.');
    $db->beginTransaction();
    try{
        $user=account_profile_lock($db,$sessionUser,$current);
        if($email===$user['email'])throw new InvalidArgumentException('Enter a different email address.');
        $q=$db->prepare('SELECT 1 FROM accounts WHERE email=? AND id<>?');$q->execute([$email,$user['id']]);if($q->fetchColumn())throw new InvalidArgumentException('That email address is already in use.');
        if($verify){
            $token=bin2hex(random_bytes(32));
            $db->prepare('INSERT INTO account_email_changes(account_id,token_hash,email,auth_version,expires) VALUES(?,?,?,?,?) ON DUPLICATE KEY UPDATE token_hash=VALUES(token_hash),email=VALUES(email),auth_version=VALUES(auth_version),expires=VALUES(expires)')->execute([$user['id'],hash('sha256',$token),$email,$user['auth_version'],time()+86400]);
            try{smtp_send($settings['smtp'],$email,'Confirm your new LineClash email',"Open this link and choose Confirm email change:\n\n".$settings['url'].'/account/email/#'.$token."\n\nThe link expires in 24 hours and works once. Your current email remains active until you confirm. If you did not request this change, ignore this email.\n");}
            catch(Throwable $e){error_log('LineClash email change: '.$e->getMessage());throw new InvalidArgumentException('Confirmation email could not be sent. Try again later.');}
        }else{
            $db->prepare('UPDATE accounts SET email=?,email_pending=0 WHERE id=?')->execute([$email,$user['id']]);account_profile_revoke($db,$user['id']);account_profile_clear_counters($db,[$user['email']]);
            $db->prepare('DELETE FROM account_email_verifications WHERE account_id=?')->execute([$user['id']]);
        }
        $db->commit();
    }catch(Throwable $e){if($db->inTransaction())$db->rollBack();if($e instanceof PDOException && $e->getCode()==='23000')throw new InvalidArgumentException('That email address is already in use.');throw $e;}
    if(!$verify)account_profile_notice($user['email'],'LineClash email changed',"Your LineClash sign-in email was changed. If you did not make this change, contact the site operator.\n");
    return $verify;
}
function account_profile_confirm_email(PDO $db,string $token): void {
    account_limit($db,'account-email-confirm',10);if(!preg_match('/^[a-f0-9]{64}$/D',$token))throw new InvalidArgumentException('This email change link is invalid or expired.');
    $db->beginTransaction();
    try{
        $q=$db->prepare('SELECT a.*,c.email AS new_email FROM account_email_changes c JOIN accounts a ON a.id=c.account_id WHERE c.token_hash=? AND c.expires>=? AND a.active=1 AND a.auth_version=c.auth_version FOR UPDATE');$q->execute([hash('sha256',$token),time()]);$user=$q->fetch();
        if(!$user)throw new InvalidArgumentException('This email change link is invalid or expired.');
        $db->prepare('UPDATE accounts SET email=?,email_pending=0 WHERE id=?')->execute([$user['new_email'],$user['id']]);account_profile_revoke($db,$user['id']);account_profile_clear_counters($db,[$user['email']]);
        $db->prepare('DELETE FROM account_email_verifications WHERE account_id=?')->execute([$user['id']]);$db->commit();
    }catch(Throwable $e){if($db->inTransaction())$db->rollBack();if($e instanceof PDOException && $e->getCode()==='23000')throw new InvalidArgumentException('That email address is already in use. Request a different change.');throw $e;}
    account_profile_notice($user['email'],'LineClash email changed',"Your LineClash sign-in email was changed. If you did not make this change, contact the site operator.\n");
}
function account_profile_delete(PDO $db,array $sessionUser,string $password,string $confirmation): void {
    if($confirmation!=='DELETE')throw new InvalidArgumentException('Type DELETE to confirm account deletion.');account_limit($db,'account-delete',5);
    $lock=privacy_lock($db);
    try{
        $db->beginTransaction();$user=account_profile_lock($db,$sessionUser,$password);
        $q=$db->prepare('SELECT email FROM account_email_changes WHERE account_id=?');$q->execute([$user['id']]);$pendingEmail=$q->fetchColumn();
        privacy_erase_history($db,$user);
        foreach(['account_sessions','account_devices','account_client_pins','account_password_resets','account_email_verifications','account_email_changes'] as $table)$db->prepare('DELETE FROM '.$table.' WHERE account_id=?')->execute([$user['id']]);
        // Remove email/username-derived counters as well; shared source counters have no account link.
        account_profile_clear_counters($db,[$user['handle'],$user['email'],$pendingEmail]);
        $db->prepare('DELETE FROM accounts WHERE id=?')->execute([$user['id']]);$db->commit();
    }catch(Throwable $e){if($db->inTransaction())$db->rollBack();throw $e;}
    finally{privacy_unlock($db,$lock);}
}
