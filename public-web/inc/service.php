<?php
declare(strict_types=1);
function web_config_path(): string {
    $override=getenv('LINECLASH_WEB_CONFIG');
    return $override!==false && $override!=='' ? $override : dirname(__DIR__).'/config.inc.php';
}
function database(): PDO {
    static $db;
    if ($db) return $db;
    $path = web_config_path();
    if (!$path || !is_file($path)) throw new RuntimeException('Service not configured');
    $c = require $path;
    $options=[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_EMULATE_PREPARES=>false, PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC];
    if(!empty($c['ssl_ca'])) {
        if(!is_string($c['ssl_ca']) || !is_readable($c['ssl_ca'])) throw new RuntimeException('Database CA file is not readable');
        $ca=defined('Pdo\\Mysql::ATTR_SSL_CA')?constant('Pdo\\Mysql::ATTR_SSL_CA'):PDO::MYSQL_ATTR_SSL_CA;
        $verify=defined('Pdo\\Mysql::ATTR_SSL_VERIFY_SERVER_CERT')?constant('Pdo\\Mysql::ATTR_SSL_VERIFY_SERVER_CERT'):PDO::MYSQL_ATTR_SSL_VERIFY_SERVER_CERT;
        $options[$ca]=$c['ssl_ca'];
        $options[$verify]=true;
    }
    return $db = new PDO($c['dsn'], $c['user'], $c['password'], $options);
}
function ttl(): int {
    $c = require web_config_path();
    return max(30, min(3600, (int)($c['ttl'] ?? 180)));
}
function live_servers(PDO $db): array {
    $q=$db->prepare('SELECT id,name,host,port,last_seen,status_json FROM directory_servers WHERE last_seen >= ? ORDER BY name,id LIMIT 500');
    $q->execute([time()-ttl()]);
    return array_map(function($r) { $r['status']=json_decode($r['status_json'], true); unset($r['status_json']); return $r; }, $q->fetchAll());
}
function text_value(array $a,string $key,int $max): string {
    if (!isset($a[$key]) || !is_string($a[$key]) || strlen($a[$key])>$max || preg_match('/[\x00-\x1f\x7f]/',$a[$key]) || !preg_match('//u',$a[$key])) throw new InvalidArgumentException('Invalid '.$key);
    return $a[$key];
}
function integer_value(array $a,string $key,int $min,int $max): int {
    if (!isset($a[$key]) || !is_int($a[$key]) || $a[$key]<$min || $a[$key]>$max) throw new InvalidArgumentException('Invalid '.$key);
    return $a[$key];
}
// Self-registration lists only the HTTP peer's own address. It never grants
// the trusted operator credentials needed to upload game results.
function public_register(PDO $db,array $a): array {
    $ip=$_SERVER['REMOTE_ADDR']??'';
    $config=require web_config_path();
    $flags=empty($config['allow_local_registration'])?FILTER_FLAG_NO_PRIV_RANGE|FILTER_FLAG_NO_RES_RANGE:0;
    if(!filter_var($ip,FILTER_VALIDATE_IP,$flags))throw new InvalidArgumentException('A public source address is required');
    $port=integer_value($a,'port',1,65535);$name=text_value($a,'name',128);
    if(trim($name)==='')throw new InvalidArgumentException('Server name required');
    $token=text_value($a,'token',64);
    if(!preg_match('/^[a-f0-9]{64}$/D',$token))throw new InvalidArgumentException('Invalid token');
    $players=integer_value($a,'players',0,80);$capacity=integer_value($a,'capacity',1,80);
    if($players>$capacity)throw new InvalidArgumentException('Invalid player count');
    $id='auto-'.substr(hash('sha256',$ip.':'.$port),0,32);
    $lock='lc-register-'.substr(hash('sha256',$ip),0,40);
    $q=$db->prepare('SELECT GET_LOCK(?,2)');$q->execute([$lock]);
    if((int)$q->fetchColumn()!==1){http_response_code(429);return ['error'=>'Try again later'];}
    try {
        $q=$db->prepare('SELECT token_hash FROM directory_servers WHERE id=?');$q->execute([$id]);$row=$q->fetch();
        if($row && !hash_equals($row['token_hash'],hash('sha256',$token))){http_response_code(403);return ['error'=>'Directory key does not match; retain the original key or contact the directory operator'];}
        if(!$row){
            $q=$db->prepare("SELECT COUNT(*) FROM directory_servers WHERE host=? AND id LIKE 'auto-%'");$q->execute([$ip]);
            if((int)$q->fetchColumn()>=8){http_response_code(429);return ['error'=>'Public registration limit reached'];}
        }
        $status=json_encode(['players'=>$players,'capacity'=>$capacity],JSON_THROW_ON_ERROR);
        $q=$db->prepare('INSERT INTO directory_servers(id,name,host,port,token_hash,last_seen,status_json) VALUES (?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE name=VALUES(name),last_seen=VALUES(last_seen),status_json=VALUES(status_json)');
        $q->execute([$id,$name,$ip,$port,hash('sha256',$token),time(),$status]);
        return ['ok'=>true,'server_id'=>$id];
    } finally {$q=$db->prepare('SELECT RELEASE_LOCK(?)');$q->execute([$lock]);}
}

function announce(PDO $db,array $a): array {
    $id=text_value($a,'server_id',64);
    $token=text_value($a,'token',256);
    $q=$db->prepare('SELECT token_hash FROM directory_servers WHERE id=?'); $q->execute([$id]); $s=$q->fetch();
    if (!$s || !hash_equals($s['token_hash'],hash('sha256',$token))) { http_response_code(403); return ['error'=>'Authentication failed']; }
    if(str_starts_with($id,'auto-')){http_response_code(403);return ['error'=>'Game reporting requires operator approval'];}
    $database=text_value($a,'database_id',32);
    if (!preg_match('/^[a-f0-9]{32}$/D',$database)) throw new InvalidArgumentException('Invalid database_id');
    $games=$a['games'] ?? null;
    if (!is_array($games) || !array_is_list($games) || count($games)>100) throw new InvalidArgumentException('Invalid games');
    $status=$a['status'] ?? [];
    if (!is_array($status)) throw new InvalidArgumentException('Invalid status');
    // Only explicitly public counts leave the server. Never persist arbitrary status objects.
    $clean=[];
    foreach (['players','rooms','users','capacity'] as $k) if(isset($status[$k])) $clean[$k]=integer_value($status,$k,0,10000000);
    $rows=[]; $playerRows=[];
    foreach($games as $g) {
        if(!is_array($g)) throw new InvalidArgumentException('Invalid game');
        $rows[]=[ $id,$database,integer_value($g,'game_id',1,PHP_INT_MAX),integer_value($g,'room_id',0,100000),
            text_value($g,'started_at',32),text_value($g,'ended_at',32),text_value($g,'winner_nick',64),text_value($g,'winner_team',64),integer_value($g,'player_count',0,1000) ];
        if(end($rows)[5]==='') throw new InvalidArgumentException('Game is unfinished');
        $index=count($rows)-1;
        $started=isset($g['started_epoch'])?integer_value($g,'started_epoch',0,4102444800):0;
        $ended=isset($g['ended_epoch'])?integer_value($g,'ended_epoch',0,4102444800):0;
        if(($started>0 && $ended<$started) || ($ended>0 && $started===0)) throw new InvalidArgumentException('Invalid UTC times');
        $reason=isset($g['end_reason'])?text_value($g,'end_reason',32):'legacy';
        if(!in_array($reason,['legacy','completed','stopped','abandoned'],true)) throw new InvalidArgumentException('Invalid end reason');
        $ranked=isset($g['ranked'])?integer_value($g,'ranked',0,1):0;
        if($ranked && ($reason!=='completed' || $started===0 || $g['player_count']<2)) throw new InvalidArgumentException('Invalid ranked match');
        $protocol=isset($g['input_protocol'])?text_value($g,'input_protocol',16):'legacy';
        if(!in_array($protocol,['legacy','input-v1'],true))throw new InvalidArgumentException('Invalid input protocol');
        if($protocol!=='input-v1')$ranked=0;
        $raw=null; $hash='';
        if(isset($g['replay_gzip'])) {
            $packed=base64_decode(text_value($g,'replay_gzip',1500000),true);
            $raw=$packed===false?false:@gzdecode($packed,1500000);
            $hash=text_value($g,'replay_sha256',64);
            if($raw===false || !preg_match('/^[a-f0-9]{64}$/D',$hash) || !hash_equals(hash('sha256',$raw),$hash)) throw new InvalidArgumentException('Replay checksum mismatch');
            $events=json_decode($raw,true,16,JSON_THROW_ON_ERROR);
            if(!is_array($events) || !array_is_list($events) || count($events)>2048) throw new InvalidArgumentException('Invalid replay');
            $last=-1;
            foreach($events as $event) {
                if(!is_array($event)) throw new InvalidArgumentException('Invalid replay event');
                $ms=integer_value($event,'elapsed_ms',0,604800000);
                $line=text_value($event,'line',511);
                if($ms<$last || !preg_match('/^(newgame |roundid |f |sb |playerlost |playerwon |pause |endgame$|playerjoin |lockdelay )/D',$line)) throw new InvalidArgumentException('Invalid replay event');
                $last=$ms;
            }
        }
        $rows[$index]=array_merge($rows[$index],[$started,$ended,$ranked,$reason,$raw,$hash,isset($g['replay_truncated'])?integer_value($g,'replay_truncated',0,1):0,$protocol]);
        $players=$g['players']??[];
        if(!is_array($players) || !array_is_list($players) || count($players)>8) throw new InvalidArgumentException('Invalid players');
        $playerRows[$index]=[]; $seen=[];
        foreach($players as $player) {
            if(!is_array($player)) throw new InvalidArgumentException('Invalid player');
            $nick=text_value($player,'nick',64);
            if($nick==='' || isset($seen[strtolower($nick)])) throw new InvalidArgumentException('Duplicate player');
            $seen[strtolower($nick)]=true;
            $playerRows[$index][]=[$id,$database,$g['game_id'],$nick,integer_value($player,'lines_cleared',0,10000000),integer_value($player,'specials_sent',0,10000000),integer_value($player,'blocks_placed',0,10000000),integer_value($player,'placement',0,1000),integer_value($player,'won',0,1)];
        }
    }
    $db->beginTransaction();
    try {
        $q=$db->prepare('UPDATE directory_servers SET last_seen=?,status_json=? WHERE id=?'); $q->execute([time(),json_encode($clean,JSON_THROW_ON_ERROR),$id]);
        $q=$db->prepare('INSERT INTO directory_games(server_id,database_id,game_id,room_id,started_at,ended_at,winner_nick,winner_team,player_count,started_epoch,ended_epoch,ranked,end_reason,replay_json,replay_sha256,replay_truncated,input_protocol) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE game_id=game_id');
        $pq=$db->prepare('INSERT INTO directory_players VALUES(?,?,?,?,?,?,?,?,?)');
        foreach($rows as $index=>$row) {
            $q->execute($row);
            if($q->rowCount()===1) foreach($playerRows[$index] as $player) $pq->execute($player);
        }
        $db->commit();
    } catch(Throwable $e) { if($db->inTransaction()) $db->rollBack(); throw $e; }
    return ['ok'=>true,'ack'=>array_column($games,'game_id')];
}

function game_filter(): array {
    $server=$_GET['server']??''; $mode=$_GET['mode']??'all';
    if(!is_string($server) || strlen($server)>64 || !in_array($mode,['all','ranked','casual'],true)) throw new InvalidArgumentException('Invalid filter');
    $where='1=1'; $args=[];
    if($server!=='') { $where.=' AND g.server_id=?'; $args[]=$server; }
    if($mode!=='all') { $where.=' AND g.ranked=?'; $args[]=$mode==='ranked'?1:0; }
    return [$where,$args];
}
function game_key(): array {
    $s=text_value($_GET,'server',64); $d=text_value($_GET,'database',32);
    $id=filter_var($_GET['game']??'',FILTER_VALIDATE_INT,['options'=>['min_range'=>1]]);
    if(!$id || !preg_match('/^[a-f0-9]{32}$/D',$d)) throw new InvalidArgumentException('Invalid game key');
    return [$s,$d,$id];
}
