<?php
declare(strict_types=1);
require __DIR__.'/../inc/service.php';
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');
$account_request=false;
function api_failure(string $message,int $status,bool $account): void {
    http_response_code($status);
    if($account){header('Content-Type: text/plain; charset=utf-8');echo 'central-error '.$message;}
    else{header('Content-Type: application/json; charset=utf-8');echo json_encode(['error'=>$message]);}
}
try {
    if($_SERVER['REQUEST_METHOD']==='POST') {
        if((int)($_SERVER['CONTENT_LENGTH']??0)>2097152) { http_response_code(413); echo '{"error":"Payload too large"}'; exit; }
        $body=file_get_contents('php://input',false,null,0,2097153);
        if(strlen($body)>2097152) { http_response_code(413); echo '{"error":"Payload too large"}'; exit; }
        $a=json_decode($body,true,32,JSON_THROW_ON_ERROR);
        if(!is_array($a)) throw new InvalidArgumentException('Expected object');
        $account_request=in_array($a['action']??'',['account-client-register','account-client-login','account-client-join','account-client-logout','account-begin','account-poll'],true);
        $db=database();
        if(in_array($a['action']??'',['account-client-register','account-client-login','account-client-join','account-client-logout'],true)){require_once __DIR__.'/../inc/accounts.php';header('Content-Type: text/plain; charset=utf-8');try{echo account_client_api($db,$a);}catch(InvalidArgumentException $e){http_response_code(str_starts_with($e->getMessage(),'Too many')?429:400);echo 'central-error '.$e->getMessage();}exit;}
        if(in_array($a['action']??'',['account-begin','account-poll'],true)){require_once __DIR__.'/../inc/accounts.php';header('Content-Type: text/plain; charset=utf-8');echo account_api($db,$a);exit;}
        $result=($a['action']??'')==='public-register'?public_register($db,$a):announce($db,$a);
    } elseif($_SERVER['REQUEST_METHOD']==='GET') {
        $db=database();
        header('Access-Control-Allow-Origin: *');
        $action=$_GET['action']??'servers';
        $offset=filter_var($_GET['offset']??0,FILTER_VALIDATE_INT,['options'=>['min_range'=>0,'max_range'=>1000000]]);
        if($offset===false) throw new InvalidArgumentException('Invalid offset');
        if($action==='servers') {
            $servers=live_servers($db);
            if(($_GET['format']??'')==='text') {
                header('Content-Type: text/plain; charset=utf-8'); echo "LC-DIRECTORY-1\n";
                foreach(array_slice($servers,0,64) as $s) {
                    if(strlen($s['host'])>127) continue;
                    $clean=fn($x)=>str_replace(["\t","\r","\n"],' ',(string)$x);
                    echo implode("\t",array_map($clean,[$s['id'],$s['name'],$s['host'],$s['port'],$s['status']['players']??0,$s['status']['capacity']??0]))."\n";
                }
                exit;
            }
            $result=['servers'=>$servers];
        }
        elseif($action==='matches') {
            [$where,$args]=game_filter();
            $q=$db->prepare('SELECT g.server_id,g.database_id,g.game_id,g.room_id,g.started_at,g.ended_at,g.started_epoch,g.ended_epoch,g.ranked,g.end_reason,g.winner_nick,g.winner_team,g.player_count,g.replay_sha256,g.replay_truncated,s.name AS server_name FROM directory_games g JOIN directory_servers s ON s.id=g.server_id WHERE '.$where.' ORDER BY g.ended_epoch DESC,g.ended_at DESC,g.server_id,g.database_id,g.game_id DESC LIMIT 50 OFFSET ?');
            foreach($args as $i=>$arg) $q->bindValue($i+1,$arg); $q->bindValue(count($args)+1,$offset,PDO::PARAM_INT); $q->execute(); $result=['matches'=>$q->fetchAll()];
        } elseif($action==='statistics') {
            $result=['games'=>(int)$db->query('SELECT COUNT(*) FROM directory_games')->fetchColumn(), 'ranked_games'=>(int)$db->query('SELECT COUNT(*) FROM directory_games WHERE ranked=1')->fetchColumn(), 'servers'=>(int)$db->query('SELECT COUNT(*) FROM directory_servers')->fetchColumn(), 'live_servers'=>count(live_servers($db))];
        } elseif($action==='leaders') {
            // Nicknames belong to a server; they are not global authenticated identities.
            $result=['leaders'=>$db->query("SELECT server_id,winner_nick,COUNT(*) AS wins FROM directory_games WHERE ranked=1 AND end_reason='completed' AND winner_nick<>'' GROUP BY server_id,winner_nick ORDER BY wins DESC,server_id,winner_nick LIMIT 50")->fetchAll()];
        } elseif($action==='match' || $action==='replay') {
            $key=game_key(); $q=$db->prepare('SELECT * FROM directory_games WHERE server_id=? AND database_id=? AND game_id=?'); $q->execute($key); $game=$q->fetch();
            if(!$game) { http_response_code(404); $result=['error'=>'Match not found']; }
            else {
                $source=$game['replay_json']; $events=$game['replay_json']!==null?json_decode($game['replay_json'],true,16,JSON_THROW_ON_ERROR):[]; unset($game['replay_json']);
                $q=$db->prepare('SELECT nick,lines_cleared,specials_sent,blocks_placed,placement,won FROM directory_players WHERE server_id=? AND database_id=? AND game_id=? ORDER BY placement=0,placement,nick'); $q->execute($key);
                $result=['match'=>$game,'players'=>$q->fetchAll()];
                if($action==='replay') { $result['events']=$events; $result['replay_source']=$source; if(($_GET['download']??'')==='1') header('Content-Disposition: attachment; filename="lineclash-replay.json"'); }
            }
        } elseif($action==='player') {
            $server=text_value($_GET,'server',64); $nick=text_value($_GET,'nick',64);
            $q=$db->prepare('SELECT p.*,g.started_epoch,g.ended_epoch,g.ranked,g.end_reason FROM directory_players p JOIN directory_games g USING(server_id,database_id,game_id) WHERE p.server_id=? AND p.nick=? ORDER BY g.ended_epoch DESC,g.game_id DESC LIMIT 50'); $q->execute([$server,$nick]);
            $result=['server'=>$server,'nick'=>$nick,'matches'=>$q->fetchAll()];
        } else throw new InvalidArgumentException('Unknown action');
    } else { http_response_code(405); header('Allow: GET, POST'); $result=['error'=>'Method not allowed']; }
    echo json_encode($result,JSON_THROW_ON_ERROR|JSON_INVALID_UTF8_SUBSTITUTE);
} catch(InvalidArgumentException|JsonException $e) { api_failure($e->getMessage(),400,$account_request); }
catch(PDOException $e) {
    error_log('LineClash database error: '.$e->getMessage());
    $migration=in_array($e->getCode(),['42S02','42S22'],true);
    $permission=in_array((int)($e->errorInfo[1]??0),[1044,1045,1142,1143],true);
    $message=$migration?'Website database migration required: run php bin/manage.php migrate':($permission?'Website database permissions are incomplete; check the Apache error log':'Account service unavailable; check the website server log');
    api_failure($message,503,$account_request);
}
catch(Throwable $e) { error_log('LineClash service error: '.$e->getMessage());api_failure('Service unavailable; check the website server log',503,$account_request); }
