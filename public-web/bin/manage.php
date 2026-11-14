<?php
declare(strict_types=1);
if(PHP_SAPI!=='cli') { http_response_code(404); exit; }
require __DIR__.'/../inc/service.php';
// Child tables precede their parents; DELETE keeps foreign keys and rollback intact.
function reset_website_data(PDO $db,bool $confirmed): void {
    $tables=['account_email_changes','account_email_verifications','account_client_pins','account_password_resets','account_sessions','account_devices','directory_players','directory_games',
             'directory_registration_limits','accounts','directory_servers'];
    $database=$db->query('SELECT DATABASE()')->fetchColumn();
    if(!is_string($database) || $database==='')throw new RuntimeException('No database selected in the site configuration');
    $q=$db->prepare('SELECT TABLE_NAME,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('.implode(',',array_fill(0,count($tables),'?')).')');
    $q->execute($tables);$engines=[];foreach($q->fetchAll() as $row)$engines[$row['TABLE_NAME']]=$row['ENGINE'];
    foreach($tables as $table){
        if(!array_key_exists($table,$engines))throw new RuntimeException('Schema incomplete: run php bin/manage.php migrate first (missing '.$table.')');
        if(strcasecmp((string)$engines[$table],'InnoDB')!==0)throw new RuntimeException('Reset requires transactional InnoDB tables: '.$table);
    }
    echo 'Database: '.$database."\n";
    if(!$confirmed){
        echo "Preview only. These LineClash website tables would be emptied:\n";
        foreach($tables as $table)echo '  '.$table.': '.$db->query('SELECT COUNT(*) FROM `'.$table.'`')->fetchColumn()." rows\n";
        echo "No data changed. To erase it, use reset-data --confirm-reset.\n";return;
    }
    $db->beginTransaction();$removed=0;
    try {
        foreach($tables as $table)$removed+=$db->exec('DELETE FROM `'.$table.'`');
        $db->commit();
    }catch(Throwable $e){if($db->inTransaction())$db->rollBack();throw new RuntimeException('Reset failed; all deletions were rolled back. '.$e->getMessage(),0,$e);}
    echo 'Removed '.$removed." rows from all LineClash website tables. Schema and local configuration preserved.\n";
    echo "Central accounts, sessions, server registrations and website game history have been erased. Dedicated-server databases were not changed.\n";
}
try {
    $command=$argv[1]??'';
    if($command==='reset-data' && !(count($argv)===2 || (count($argv)===3 && in_array($argv[2],['--dry-run','--confirm-reset'],true))))throw new RuntimeException('Usage: php bin/manage.php reset-data [--dry-run | --confirm-reset]');
    if($command==='smtp-test'){
        if(count($argv)!==3)throw new RuntimeException('Usage: php bin/manage.php smtp-test EMAIL');
        require_once __DIR__.'/../inc/smtp.php';$smtp=smtp_settings(require web_config_path());
        if(!$smtp)throw new RuntimeException('SMTP is not configured or its settings are invalid');
        smtp_send($smtp,$argv[2],'LineClash SMTP test',"SMTP delivery from LineClash is working. This message contains no account information.\n");
        echo "SMTP relay accepted the test message. Check the recipient inbox.\n";exit;
    }
    $db=database();
    if(($argv[1]??'')==='migrate') {
        $db->exec(file_get_contents(__DIR__.'/../inc/schema.sql')); echo "Schema ready.\n";
    } elseif($command==='reset-data') {
        reset_website_data($db,($argv[2]??'')==='--confirm-reset');
    } elseif(($argv[1]??'')==='server' && count($argv)===6) {
        [,,$id,$host,$port,$name]=$argv;
        if(!preg_match('/^[a-zA-Z0-9_-]{1,64}$/D',$id) || !preg_match('/^[a-zA-Z0-9.:-]{1,253}$/D',$host) || !ctype_digit($port) || (int)$port<1 || (int)$port>65535 || strlen($name)>128) throw new RuntimeException('Invalid server fields');
        $token=bin2hex(random_bytes(32));
        $q=$db->prepare("INSERT INTO directory_servers(id,name,host,port,token_hash,status_json) VALUES(?,?,?,?,?,'{}') ON DUPLICATE KEY UPDATE name=VALUES(name),host=VALUES(host),port=VALUES(port),token_hash=VALUES(token_hash),last_seen=0");
        $q->execute([$id,$name,$host,(int)$port,hash('sha256',$token)]);
        echo "Set LINECLASH_DIRECTORY_TOKEN to: $token\n";
    } else throw new RuntimeException('Usage: php bin/manage.php migrate | smtp-test EMAIL | server ID HOST PORT NAME | reset-data [--dry-run | --confirm-reset]');
} catch(Throwable $e) { fwrite(STDERR,$e->getMessage()."\n"); exit(1); }
