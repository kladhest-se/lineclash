<?php
// SPDX-License-Identifier: MIT
// CLI maintenance only: preserve match results while erasing central identities.
declare(strict_types=1);
require_once __DIR__.'/account-privacy.php';
function user_reset_tables(PDO $db): array {
    $tables=['account_email_changes','account_email_verifications','account_client_pins','account_password_resets','account_sessions','account_devices','directory_players','directory_games','directory_registration_limits','accounts','directory_servers'];
    $database=$db->query('SELECT DATABASE()')->fetchColumn();
    if(!is_string($database) || $database==='')throw new RuntimeException('No database selected in the site configuration');
    $q=$db->prepare('SELECT TABLE_NAME,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('.implode(',',array_fill(0,count($tables),'?')).')');
    $q->execute($tables);$engines=[];foreach($q->fetchAll() as $row)$engines[$row['TABLE_NAME']]=$row['ENGINE'];
    foreach($tables as $table){
        if(!isset($engines[$table]))throw new RuntimeException('Schema incomplete: run php bin/manage.php migrate first (missing '.$table.')');
        if(strcasecmp((string)$engines[$table],'InnoDB')!==0)throw new RuntimeException('User reset requires transactional InnoDB tables: '.$table);
    }
    return [$database,$tables];
}
function user_reset_backup_open(string $path) {
    $parent=realpath(dirname($path));
    if($path==='' || !str_starts_with($path,DIRECTORY_SEPARATOR) || $parent===false)throw new RuntimeException('Backup requires an absolute path in an existing private directory');
    // The deployed configuration may live outside this copy of the application.
    foreach([realpath(dirname(__DIR__)),realpath(dirname(web_config_path()))] as $web){
        if($web!==false && ($parent===$web || str_starts_with($parent,$web.DIRECTORY_SEPARATOR)))throw new RuntimeException('Backup must be outside the website document root');
    }
    $mask=umask(0077);
    try{$file=@fopen($path,'xb');}finally{umask($mask);}
    if($file===false)throw new RuntimeException('Cannot create private SQL backup (existing files are never overwritten): '.$path);
    if(!chmod($path,0600)){fclose($file);unlink($path);throw new RuntimeException('Cannot protect SQL backup permissions');}
    return $file;
}
function user_reset_write($file,string $text): void {
    while($text!==''){
        $written=fwrite($file,$text);
        if($written===false || $written===0)throw new RuntimeException('SQL backup write failed; no user data was erased');
        $text=substr($text,$written);
    }
}
function user_reset_backup(PDO $db,$file,string $database,array $tables): void {
    $mode=(string)$db->query('SELECT @@SESSION.sql_mode')->fetchColumn();
    user_reset_write($file,'-- LineClash website data before reset-users; database: '.str_replace(["\r","\n"],' ',$database)."\n-- Restore into the matching migrated schema with mariadb DATABASE < this-file.sql\nSET NAMES utf8mb4;\nSET @LINECLASH_OLD_SQL_MODE=@@SESSION.sql_mode;\nSET SESSION sql_mode=".$db->quote($mode).";\nSTART TRANSACTION;\n");
    foreach($tables as $table)user_reset_write($file,'DELETE FROM `'.$table."`;\n");
    // Parent rows must exist before their foreign-key children on restoration.
    foreach(array_reverse($tables) as $table){
        $q=$db->query('SELECT * FROM `'.$table.'` FOR UPDATE');
        while($row=$q->fetch(PDO::FETCH_ASSOC)){
            $columns=array_map(fn($name)=>'`'.str_replace('`','``',$name).'`',array_keys($row));
            $values=array_map(fn($value)=>$value===null?'NULL':$db->quote((string)$value),array_values($row));
            user_reset_write($file,'INSERT INTO `'.$table.'` ('.implode(',',$columns).') VALUES ('.implode(',',$values).");\n");
        }
    }
    user_reset_write($file,"COMMIT;\nSET SESSION sql_mode=@LINECLASH_OLD_SQL_MODE;\n");
    if(!fflush($file) || (function_exists('fsync') && !fsync($file)))throw new RuntimeException('Could not flush SQL backup; no user data was erased');
}
function reset_central_users(PDO $db,bool $confirmed,string $backup=''): void {
    [$database,$tables]=user_reset_tables($db);
    $accountTables=['account_email_changes','account_email_verifications','account_client_pins','account_password_resets','account_sessions','account_devices','accounts'];
    echo 'Database: '.$database."\n";
    if(!$confirmed){
        echo "Preview only. Central accounts and private sign-in records would be removed:\n";
        foreach($accountTables as $table)echo '  '.$table.': '.$db->query('SELECT COUNT(*) FROM `'.$table.'`')->fetchColumn()." rows\n";
        echo "Player identities in retained games/replays would be anonymized. Shared rate counters would be cleared.\nServer registrations, match scores, team results, schema and configuration are preserved.\nNo data changed. To erase users, use reset-users --confirm-reset --backup /private/path/users.sql\n";
        return;
    }
    $file=user_reset_backup_open($backup);$complete=false;$lock='';
    try{
        $lock=privacy_lock($db);
        $db->exec('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ');$db->beginTransaction();
        // Lock identities first, consistent with normal account deletion.
        $users=$db->query('SELECT id,handle,email FROM accounts ORDER BY id FOR UPDATE')->fetchAll();
        user_reset_backup($db,$file,$database,$tables);$complete=true;fclose($file);$file=null;
        echo 'Private SQL backup: '.$backup."\n";
        foreach($users as $user)privacy_erase_history($db,$user);
        foreach($accountTables as $table)$db->exec('DELETE FROM `'.$table.'`');
        $db->exec('DELETE FROM directory_registration_limits');
        $db->commit();
        echo 'Removed '.count($users)." central accounts and all sign-in/recovery records.\nPlayer names were anonymized; match results and server registrations were retained.\n";
    }catch(Throwable $e){
        if($db->inTransaction())$db->rollBack();
        throw new RuntimeException('User reset failed; all database changes were rolled back. '.$e->getMessage(),0,$e);
    }finally{
        if(is_resource($file))fclose($file);
        if(!$complete)@unlink($backup);
        if($lock!=='')privacy_unlock($db,$lock);
    }
}
