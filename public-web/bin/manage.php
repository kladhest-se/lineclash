<?php
declare(strict_types=1);
if(PHP_SAPI!=='cli') { http_response_code(404); exit; }
require __DIR__.'/../inc/service.php';
try {
    $db=database();
    if(($argv[1]??'')==='migrate') {
        $db->exec(file_get_contents(__DIR__.'/../inc/schema.sql')); echo "Schema ready.\n";
    } elseif(($argv[1]??'')==='server' && count($argv)===6) {
        [,,$id,$host,$port,$name]=$argv;
        if(!preg_match('/^[a-zA-Z0-9_-]{1,64}$/D',$id) || !preg_match('/^[a-zA-Z0-9.:-]{1,253}$/D',$host) || !ctype_digit($port) || (int)$port<1 || (int)$port>65535 || strlen($name)>128) throw new RuntimeException('Invalid server fields');
        $token=bin2hex(random_bytes(32));
        $q=$db->prepare("INSERT INTO directory_servers(id,name,host,port,token_hash,status_json) VALUES(?,?,?,?,?,'{}') ON DUPLICATE KEY UPDATE name=VALUES(name),host=VALUES(host),port=VALUES(port),token_hash=VALUES(token_hash),last_seen=0");
        $q->execute([$id,$name,$host,(int)$port,hash('sha256',$token)]);
        echo "Set LINECLASH_DIRECTORY_TOKEN to: $token\n";
    } else throw new RuntimeException('Usage: php bin/manage.php migrate | server ID HOST PORT NAME');
} catch(Throwable $e) { fwrite(STDERR,$e->getMessage()."\n"); exit(1); }
