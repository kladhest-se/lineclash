<?php
declare(strict_types=1);
require_once __DIR__.'/site.php';require_once __DIR__.'/account-privacy.php';
function community_load(string $page='home'): array {
    $d=['available'=>false,'invalid'=>false,'servers'=>[],'all_servers'=>[],'matches'=>[],'leaders'=>[],'teams'=>[],'metrics'=>[],'totals'=>[],'server_totals'=>[],'offset'=>0,'more_matches'=>false,'more_leaders'=>false,'server'=>'','mode'=>'all','q'=>'','availability'=>'all'];
    try {
        if(in_array($page,['matches','leaderboards','directory','statistics'],true)) {
            [$where,$args]=game_filter();$d['server']=$_GET['server']??'';$d['mode']=$_GET['mode']??'all';
            $offset=filter_var($_GET['offset']??0,FILTER_VALIDATE_INT,['options'=>['min_range'=>0,'max_range'=>1000000]]);
            if($offset===false)throw new InvalidArgumentException('Invalid page');$d['offset']=$offset;
        } else { $where='1=1';$args=[]; }
        if($page==='servers') {
            $q=$_GET['q']??'';$availability=$_GET['availability']??'all';
            if(!is_string($q) || strlen($q)>128 || !in_array($availability,['all','open'],true))throw new InvalidArgumentException('Invalid filter');
            $d['q']=trim($q);$d['availability']=$availability;
        }
        $db=database();$d['servers']=live_servers($db);
        $d['all_servers']=$db->query('SELECT id,name FROM directory_servers ORDER BY name,id LIMIT 500')->fetchAll();
        $d['metrics']=$db->query('SELECT COUNT(*) AS games,COALESCE(SUM(ranked=1),0) AS ranked_games FROM directory_games')->fetch();
        $d['metrics']['players_known']=count(array_filter($d['servers'],fn($s)=>!isset($s['status']['players'])))===0;
        $d['metrics']['live_servers']=count($d['servers']);$d['metrics']['players']=array_sum(array_map(fn($s)=>(int)($s['status']['players']??0),$d['servers']));
        if(in_array($page,['home','directory','matches','statistics'],true)) {
            $limit=$page==='home'?6:51;
            $sql='SELECT g.server_id,g.database_id,g.game_id,g.room_id,g.started_epoch,g.ended_epoch,g.ended_at,g.ranked,g.end_reason,g.winner_nick,g.winner_team,g.winner_deleted,g.player_count,s.name AS server_name FROM directory_games g JOIN directory_servers s ON s.id=g.server_id WHERE '.$where.' ORDER BY g.ended_epoch DESC,g.ended_at DESC,g.server_id,g.database_id,g.game_id DESC LIMIT '.$limit.' OFFSET ?';
            $stmt=$db->prepare($sql);foreach($args as $i=>$value)$stmt->bindValue($i+1,$value);$stmt->bindValue(count($args)+1,$d['offset'],PDO::PARAM_INT);$stmt->execute();$d['matches']=array_map('privacy_public_match',$stmt->fetchAll());
            $d['more_matches']=count($d['matches'])>50;$d['matches']=array_slice($d['matches'],0,50);
        }
        if(in_array($page,['home','directory','leaderboards','statistics'],true)) {
            $leaderWhere="g.ranked=1 AND g.end_reason='completed' AND g.winner_nick<>''";$leaderArgs=[];
            if($d['server']!==''){$leaderWhere.=' AND g.server_id=?';$leaderArgs[]=$d['server'];}
            $limit=$page==='home'?5:51;
            $stmt=$db->prepare('SELECT g.server_id,g.winner_nick,g.winner_deleted,s.name AS server_name,COUNT(*) AS wins FROM directory_games g JOIN directory_servers s ON s.id=g.server_id WHERE '.$leaderWhere.' GROUP BY g.server_id,g.winner_nick,g.winner_deleted,s.name ORDER BY wins DESC,g.server_id,g.winner_nick LIMIT '.$limit.' OFFSET ?');
            foreach($leaderArgs as $i=>$value)$stmt->bindValue($i+1,$value);$stmt->bindValue(count($leaderArgs)+1,$page==='leaderboards'?$d['offset']:0,PDO::PARAM_INT);$stmt->execute();$d['leaders']=array_map('privacy_public_match',$stmt->fetchAll());
            $d['more_leaders']=count($d['leaders'])>50;$d['leaders']=array_slice($d['leaders'],0,50);
        }
        if($page==='statistics') {
            $d['teams']=$db->query("SELECT g.server_id,g.winner_team,s.name AS server_name,COUNT(*) AS wins FROM directory_games g JOIN directory_servers s ON s.id=g.server_id WHERE g.ranked=1 AND g.end_reason='completed' AND g.winner_team<>'' GROUP BY g.server_id,g.winner_team,s.name ORDER BY wins DESC,g.server_id,g.winner_team LIMIT 50")->fetchAll();
            $d['totals']=$db->query('SELECT COALESCE(SUM(lines_cleared),0) AS cleared_lines,COALESCE(SUM(specials_sent),0) AS specials,COALESCE(SUM(blocks_placed),0) AS blocks FROM directory_players')->fetch();
            $d['totals']['profiles']=$db->query('SELECT COUNT(*) FROM (SELECT server_id,nick FROM directory_players GROUP BY server_id,nick) AS profiles')->fetchColumn();
            $d['totals']['average_duration']=$db->query('SELECT AVG(ended_epoch-started_epoch) FROM directory_games WHERE started_epoch>0 AND ended_epoch>=started_epoch')->fetchColumn();
            $d['server_totals']=$db->query('SELECT s.id,s.name,COUNT(g.game_id) AS games,COALESCE(SUM(g.ranked=1),0) AS ranked_games FROM directory_servers s LEFT JOIN directory_games g ON g.server_id=s.id GROUP BY s.id,s.name ORDER BY games DESC,s.name LIMIT 100')->fetchAll();
        }
        $d['available']=true;
    } catch(InvalidArgumentException $e){$d['invalid']=true;} catch(Throwable $e){error_log('LineClash community data unavailable: '.$e->getMessage());}
    return $d;
}
function community_notice(array $d): bool {
    if($d['available'])return false;
    echo '<div class="notice" role="status">'.($d['invalid']?'The selected filter or page is invalid. Please reset your filters.':'Community data is temporarily unavailable. You can still get the game and explore LineClash.').'</div>';return true;
}
function community_metrics(array $d): void { ?>
<div class="metrics" aria-label="Community statistics"><?php foreach(['live_servers'=>'Live servers','players'=>'Players online','games'=>'Recorded matches','ranked_games'=>'Ranked matches'] as $key=>$label): ?><div class="metric"><strong><?=($d['available'] && ($key!=='players' || $d['metrics']['players_known']))?number_format((int)$d['metrics'][$key]):'—'?></strong><span><?=site_h($label)?></span></div><?php endforeach; ?></div>
<?php }
function community_empty(string $title,string $message): void { ?><div class="empty-state"><h3><?=site_h($title)?></h3><p><?=site_h($message)?></p></div><?php }
function community_servers(array $servers,int $limit=500): void {
    if(!$servers){community_empty('The next match starts with you.','No servers match this view right now. Try another search, or check back when a community server comes online.');return;}
?><div class="server-grid"><?php foreach(array_slice($servers,0,$limit) as $s): $address=$s['host'].':'.$s['port'];$players=isset($s['status']['players'])?(int)$s['status']['players']:null;$capacity=(int)($s['status']['capacity']??0); ?>
<article class="server-card"><div class="server-meta"><span class="status-pill">Online</span><span><?=max(0,time()-(int)$s['last_seen'])?>s ago</span></div><h3><?=site_h($s['name'])?></h3><code><?=site_h($address)?></code><p class="population"><strong><?=$players!==null?number_format($players):'—'?></strong><?=$capacity>0?' / '.number_format($capacity):''?> players <span aria-hidden="true">·</span> <?=site_h($s['status']['rooms']??'—')?> rooms</p><button type="button" class="copy-button" data-copy-address="<?=site_h($address)?>" aria-label="Copy address for <?=site_h($s['name'])?>">Copy address</button><span class="copy-feedback" role="status"></span></article>
<?php endforeach; ?></div><?php }
function community_player_url(string $server,string $nick,string $base): string { return $base.'directory/player.php?'.http_build_query(['server'=>$server,'nick'=>$nick]); }
function community_match_url(array $g,string $base): string { return $base.'directory/match.php?'.http_build_query(['server'=>$g['server_id'],'database'=>$g['database_id'],'game'=>$g['game_id']]); }
function community_leaders(array $rows,string $base='../',int $offset=0): void {
    if(!$rows){community_empty('A place on the board is waiting.','Ranked wins will appear here after eligible competitive matches finish. Casual and stopped rounds stay in match history.');return;}
?><div class="data-panel table-scroll"><table class="community-table"><thead><tr><th scope="col">Rank</th><th scope="col">Player / server</th><th scope="col">Wins</th></tr></thead><tbody><?php foreach($rows as $i=>$p): ?><tr><td><span class="rank <?=$offset+$i===0?'first':''?>"><?=str_pad((string)($offset+$i+1),2,'0',STR_PAD_LEFT)?></span></td><td><?php if(!empty($p['winner_deleted'])):?>DELETED PLAYER<?php else:?><a href="<?=site_h(community_player_url($p['server_id'],$p['winner_nick'],$base))?>"><?=site_h($p['winner_nick'])?></a><?php endif;?><small><?=site_h($p['server_name'])?></small></td><td><?=number_format((int)$p['wins'])?></td></tr><?php endforeach; ?></tbody></table></div><?php }
function community_matches(array $rows,string $base='../'): void {
    if(!$rows){community_empty('Every rivalry has a first round.','Completed matches, results and available replays will appear here. Try another filter or check back after a game.');return;}
?><div class="data-panel table-scroll"><table class="community-table"><thead><tr><th scope="col">Match / server</th><th scope="col">Mode</th><th scope="col">Winner</th><th scope="col">Finished</th></tr></thead><tbody><?php foreach($rows as $g): ?><tr><td><a href="<?=site_h(community_match_url($g,$base))?>">Match #<?=site_h($g['game_id'])?></a><small><?=site_h($g['server_name'])?> · Room <?=site_h($g['room_id'])?> · <?=site_h($g['player_count'])?> players</small></td><td><span class="mode-pill <?=((int)$g['ranked'])?'ranked':''?>"><?=((int)$g['ranked'])?'Ranked':'Casual'?></span><small><?=site_h($g['end_reason'])?></small></td><td><?php if(!empty($g['winner_deleted'])): ?>DELETED PLAYER<?php elseif($g['winner_nick']!==''): ?><a href="<?=site_h(community_player_url($g['server_id'],$g['winner_nick'],$base))?>"><?=site_h($g['winner_nick'])?></a><?php else: ?>No winner<?php endif; ?><?php if(($g['winner_team']??'')!==''):?><small>Team <?=site_h($g['winner_team'])?></small><?php endif;?></td><td><?=site_h($g['ended_epoch']?gmdate('M j, H:i',(int)$g['ended_epoch']).' UTC':$g['ended_at'].' (legacy)')?><small><a href="<?=site_h(community_match_url($g,$base))?>">Details &amp; replay →</a></small></td></tr><?php endforeach; ?></tbody></table></div><?php }
function community_server_select(array $d): void { ?><label>Server<select name="server"><option value="">All servers</option><?php foreach($d['all_servers'] as $s): ?><option value="<?=site_h($s['id'])?>"<?=$s['id']===$d['server']?' selected':''?>><?=site_h($s['name'])?></option><?php endforeach; ?></select></label><?php }
function community_pagination(array $d,bool $more): void {
    $query=['server'=>$d['server'],'mode'=>$d['mode']];
?><nav class="pagination" aria-label="Pagination"><span><?php if($d['offset']>0): ?><a href="?<?=site_h(http_build_query($query+['offset'=>max(0,$d['offset']-50)]))?>">← Previous</a><?php endif; ?></span><span class="muted">Page <?=intdiv($d['offset'],50)+1?></span><span><?php if($more): ?><a href="?<?=site_h(http_build_query($query+['offset'=>$d['offset']+50]))?>">Next →</a><?php endif; ?></span></nav>
<?php }

function community_teams(array $teams): void {
    if(!$teams){community_empty('Team rivalries start here.','Completed ranked team wins will appear here.');return;}
    echo '<div class="data-panel table-scroll"><table class="community-table"><thead><tr><th>Team / server</th><th>Wins</th></tr></thead><tbody>';
    foreach($teams as $team)echo '<tr><td>'.site_h($team['winner_team']).'<small>'.site_h($team['server_name']).'</small></td><td>'.number_format((int)$team['wins']).'</td></tr>';
    echo '</tbody></table></div>';
}
