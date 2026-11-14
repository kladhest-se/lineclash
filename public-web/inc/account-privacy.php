<?php
// SPDX-License-Identifier: MIT
declare(strict_types=1);
require_once __DIR__.'/service.php';
function privacy_lock(PDO $db): string {
    $name='lineclash-privacy-'.substr(hash('sha256',(string)$db->query('SELECT DATABASE()')->fetchColumn()),0,32);
    $q=$db->prepare('SELECT GET_LOCK(?,10)');$q->execute([$name]);
    if((int)$q->fetchColumn()!==1)throw new RuntimeException('Account privacy update is busy; try again');
    return $name;
}
function privacy_unlock(PDO $db,string $name): void {$db->prepare('SELECT RELEASE_LOCK(?)')->execute([$name]);}
function privacy_account_id(mixed $value): ?string {
    if($value===null || $value==='')return null;
    if(!is_string($value) || !preg_match('/^[a-f0-9]{32}$/D',$value))throw new InvalidArgumentException('Invalid player account_id');
    return $value;
}
function privacy_central(string $nick): bool {return preg_match('/^@[a-z][a-z0-9_]{2,13}$/iD',$nick)===1;}
function privacy_identity_exists(PDO $db,?string $id,string $nick): bool {
    if(!$id || !privacy_central($nick))return false;
    $q=$db->prepare('SELECT 1 FROM accounts WHERE id=? AND handle=?');$q->execute([$id,substr($nick,1)]);return (bool)$q->fetchColumn();
}
function privacy_anonymous_key(string $server='',?string $id=null,string $nick=''): string {
    // High-entropy account IDs permit stable anonymous totals without a retained lookup.
    return '~deleted-'.($id!==null?substr(hash('sha256','lineclash-erased-v1:'.$server.'|'.$id.'|'.strtolower($nick)),0,32):bin2hex(random_bytes(16)));
}
function privacy_replay(?string $source,callable $redact): array {
    if($source===null)return [null,''];
    $events=json_decode($source,true,16,JSON_THROW_ON_ERROR);$clean=[];$changed=false;
    if(!is_array($events) || !array_is_list($events))throw new RuntimeException('Invalid stored replay');
    foreach($events as $event){
        if(!is_array($event) || !is_string($event['line']??null) || !is_int($event['elapsed_ms']??null))throw new RuntimeException('Invalid stored replay event');
        $line=$redact($event['line']);$clean[]=['elapsed_ms'=>$event['elapsed_ms'],'line'=>$line];
        if($line!==$event['line'] || count($event)!==2)$changed=true;
    }
    if($changed)$source=json_encode($clean,JSON_THROW_ON_ERROR|JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    return [$source,hash('sha256',$source)];
}
function privacy_central_text(string $text,callable $replacement): string {
    return preg_replace_callback('/(?<![a-z0-9_.%+\-])@[a-z][a-z0-9_]{2,13}(?![a-z0-9_])/i',fn($m)=>$replacement($m[0]),$text);
}
function privacy_emails(string $text): string {
    if(filter_var(trim($text),FILTER_VALIDATE_EMAIL))return 'EMAIL_REMOVED';
    return preg_replace("/[a-z0-9.!#\$%&'*+\/=?^_`{|}~-]+@[a-z0-9.-]+\.[a-z]{2,63}/i",'EMAIL_REMOVED',$text);
}
function privacy_team(PDO $db,string $server,string $team): string {
    $q=$db->prepare('SELECT token_hash FROM directory_servers WHERE id=?');$q->execute([$server]);$secret=$q->fetchColumn();
    if(!is_string($secret))throw new RuntimeException('Team identity unavailable');
    // A keyed, non-name team label preserves grouping without publishing the old label.
    return 'DELETED TEAM '.substr(hash_hmac('sha256','lineclash-private-team-v1:'.$team,$secret),0,24);
}
function privacy_ingest(PDO $db,array &$game,array &$players): void {
    $known=[];$anonymous=[];$ids=[];
    foreach($players as &$p){
        $original=$p[3];$id=$p[9];
        if(privacy_central($original)){
            $key=strtolower($original);$ids[$key]=$id??'';
            $known[$key]=privacy_identity_exists($db,$id,$original);
            if(!$known[$key]){$p[3]=$anonymous[$key]=privacy_anonymous_key($game[0],$id,$original);$p[9]=null;$p[10]=1;}
        }elseif($id!==null)throw new InvalidArgumentException('Only central players may carry an account ID');
    }unset($p);
    $winner=$game[6];
    if(privacy_central($winner)){
        $key=strtolower($winner);$id=$game[17]??null;
        if(!$id){foreach($players as $p)if(strcasecmp($p[3],$winner)===0)$id=$p[9];}
        // The original ID is also needed if the winning player's row was just anonymized.
        $id=$id?:($ids[$key]??null);$ids[$key]=$id??'';
        $known[$key]=privacy_identity_exists($db,$id,$winner);
        if($known[$key])$game[17]=$id;
        else{$game[6]=$anonymous[$key]??privacy_anonymous_key($game[0],$id,$winner);$game[17]=null;$game[18]=1;}
    }elseif($game[17]!==null)throw new InvalidArgumentException('Only central winners may carry an account ID');
    $redact=function(string $line)use($known): string {
        $line=privacy_emails(privacy_central_text($line,fn($nick)=>!empty($known[strtolower($nick)])?$nick:'DELETED_PLAYER'));
        if(preg_match('/^playerjoin ([1-8]) DELETED_PLAYER(?: |$)/',$line,$m))$line='playerjoin '.$m[1].' DELETED_PLAYER -';
        return $line;
    };
    $team=$redact($game[7]);
    foreach($known as $name=>$exists)if(!$exists && strcasecmp($game[7],substr($name,1))===0)$team='DELETED_TEAM';
    if($team!==$game[7])$team=privacy_team($db,$game[0],$game[7]);
    $game[7]=$team;
    $originalSource=$game[13];[$game[13],$game[14]]=privacy_replay($game[13],$redact);$game[19]=$originalSource!==$game[13]?1:0;
}
function privacy_public_player(array $row): array {
    $row['deleted']=(bool)($row['deleted']??false);
    if($row['deleted'])$row['nick']='DELETED PLAYER';
    unset($row['account_id']);return $row;
}
function privacy_public_match(array $row): array {
    $row['winner_deleted']=(bool)($row['winner_deleted']??false);
    if($row['winner_deleted'])$row['winner_nick']='DELETED PLAYER';
    unset($row['winner_account_id']);return $row;
}
function privacy_erase_history(PDO $db,array $user): void {
    $nick='@'.$user['handle'];$email=$user['email']??'';$after=['','',0];
    $redact=function(string $line)use($nick,$email): string {
        $line=privacy_central_text($line,fn($name)=>strcasecmp($name,$nick)===0?'DELETED_PLAYER':$name);
        if($email!=='')$line=str_ireplace($email,'EMAIL_REMOVED',$line);
        $line=privacy_emails($line);
        if(preg_match('/^playerjoin ([1-8]) DELETED_PLAYER(?: |$)/',$line,$m))$line='playerjoin '.$m[1].' DELETED_PLAYER -';
        return $line;
    };
    // Scan bounded batches, including replay-only references and escaped JSON names.
    for(;;){
        $q=$db->prepare('SELECT * FROM directory_games WHERE (server_id,database_id,game_id)>(?,?,?) ORDER BY server_id,database_id,game_id LIMIT 10');$q->execute($after);$batch=$q->fetchAll();if(!$batch)break;
        foreach($batch as $g){
            $key=[$g['server_id'],$g['database_id'],$g['game_id']];$after=$key;$winner=$g['winner_nick'];$winnerId=$g['winner_account_id'];$deleted=(int)$g['winner_deleted'];
            $q=$db->prepare('SELECT nick,account_id FROM directory_players WHERE server_id=? AND database_id=? AND game_id=?');$q->execute($key);
            foreach($q->fetchAll() as $p){
                if($p['account_id']!==$user['id'] && strcasecmp($p['nick'],$nick)!==0)continue;
                $alias=privacy_anonymous_key($g['server_id'],$user['id'],$nick);
                $db->prepare('UPDATE directory_players SET nick=?,account_id=NULL,deleted=1 WHERE server_id=? AND database_id=? AND game_id=? AND nick=?')->execute(array_merge([$alias],$key,[$p['nick']]));
                if(strcasecmp($winner,$p['nick'])===0){$winner=$alias;$winnerId=null;$deleted=1;}
            }
            if($winnerId===$user['id'] || strcasecmp($winner,$nick)===0){$winner=privacy_anonymous_key($g['server_id'],$user['id'],$nick);$winnerId=null;$deleted=1;}
            $team=$redact($g['winner_team']);if($team!==$g['winner_team'] || strcasecmp($g['winner_team'],$user['handle'])===0)$team=privacy_team($db,$g['server_id'],$g['winner_team']);
            [$source,$hash]=privacy_replay($g['replay_json'],$redact);
            $db->prepare('UPDATE directory_games SET winner_nick=?,winner_account_id=?,winner_deleted=?,winner_team=?,replay_json=?,replay_sha256=?,replay_redacted=? WHERE server_id=? AND database_id=? AND game_id=?')->execute(array_merge([$winner,$winnerId,$deleted,$team,$source,$hash,((int)$g['replay_redacted'] || $source!==$g['replay_json'])?1:0],$key));
        }
    }
}
